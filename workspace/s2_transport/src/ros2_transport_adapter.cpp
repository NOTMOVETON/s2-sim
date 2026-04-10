/**
 * @file ros2_transport_adapter.cpp
 * ROS2 реализация ITransportAdapter.
 */

#ifdef S2_WITH_ROS2

#include <s2/ros2_transport_adapter.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <Eigen/Geometry>

namespace s2
{

// ─── Вспомогательная функция: имя топика из типа и имени сенсора ─────────────

static std::string make_sensor_topic(const std::string& sensor_type,
                                     const std::string& sensor_name,
                                     const std::string& suffix)
{
    // gnss  ""     → /gnss/fix
    // gnss  "left" → /gnss/left/fix
    // imu   ""     → /imu/data
    // imu   "rear" → /imu/rear/data
    // diff_drive "" → /odom
    if (sensor_name.empty())
        return "/" + sensor_type + "/" + suffix;
    else
        return "/" + sensor_type + "/" + sensor_name + "/" + suffix;
}

// Суффикс топика по типу сенсора
static std::string sensor_suffix(const std::string& sensor_type)
{
    if (sensor_type == "gnss")       return "fix";
    if (sensor_type == "imu")        return "data";
    if (sensor_type == "diff_drive") return "odom";
    return "data";
}

// frame_id для оdom-топика и child_frame_id
static std::string odom_child_frame(const std::string& sensor_name)
{
    return sensor_name.empty() ? "base_link" : sensor_name + "_base_link";
}

// ─── Конструктор / деструктор ──────────────────────────────────────────────

Ros2TransportAdapter::Ros2TransportAdapter()
{
}

Ros2TransportAdapter::~Ros2TransportAdapter()
{
    stop();
}

// ─── Lifecycle ─────────────────────────────────────────────────────────────

void Ros2TransportAdapter::start()
{
    running_ = true;

    for (auto& [domain_id, info] : domain_nodes_)
    {
        spin_threads_.emplace_back([this, domain_id_copy = domain_id]()
        {
            while (running_)
            {
                auto& node_info = domain_nodes_.at(domain_id_copy);
                node_info.executor->spin_some(std::chrono::milliseconds(10));
            }
        });

        std::cout << "[Ros2TransportAdapter] Transport thread started for domain "
                  << domain_id << std::endl;
    }
}

void Ros2TransportAdapter::stop()
{
    running_ = false;

    for (auto& t : spin_threads_)
    {
        if (t.joinable()) t.join();
    }
    spin_threads_.clear();

    for (auto& [domain_id, info] : domain_nodes_)
    {
        if (info.context && info.context->is_valid())
        {
            info.context->shutdown("Ros2TransportAdapter::stop");
        }
    }
    domain_nodes_.clear();
}

// ─── Инициализация ─────────────────────────────────────────────────────────

void Ros2TransportAdapter::set_geo_origin(const GeoOrigin& origin)
{
    geo_origin_     = origin;
    geo_origin_set_ = origin.is_set();
}

void Ros2TransportAdapter::register_agent(AgentId id, int domain_id,
                                          const std::string& name,
                                          const Pose3D& initial_pose)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& info = get_or_create_node(domain_id);

    if (info.initialized)
    {
        std::cout << "[Ros2TransportAdapter] Agent " << id
                  << " (" << name << ") added to existing domain " << domain_id
                  << std::endl;
        return;
    }

    // Сохраняем начальную позу — используется в earth→map и odom→base_link
    info.initial_pose = initial_pose;

    // TF broadcasters
    info.tf_broadcaster        = std::make_shared<tf2_ros::TransformBroadcaster>(info.node);
    info.static_tf_broadcaster = std::make_shared<tf2_ros::StaticTransformBroadcaster>(info.node);

    // Статические TF (earth→map, map→odom)
    setup_static_tf(info);

    info.initialized = true;

    std::cout << "[Ros2TransportAdapter] Agent " << id
              << " (" << name << ") registered in domain " << domain_id
              << ", spawn pose: (" << initial_pose.x << ", " << initial_pose.y
              << ", " << initial_pose.z << ")" << std::endl;
}

void Ros2TransportAdapter::register_sensor(SensorRegistration reg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = domain_nodes_.find(reg.domain_id);
    if (it == domain_nodes_.end())
    {
        std::cerr << "[Ros2TransportAdapter] register_sensor: unknown domain "
                  << reg.domain_id << std::endl;
        return;
    }
    auto& info = it->second;

    const std::string& stype = reg.sensor_type;
    const std::string& sname = reg.sensor_name;

    if (stype == "gnss")
    {
        std::string topic = reg.topic_override.empty()
            ? make_sensor_topic(stype, sname, "fix")
            : reg.topic_override;
        if (info.gnss_pubs.find(sname) == info.gnss_pubs.end())
        {
            info.gnss_pubs[sname] =
                info.node->create_publisher<sensor_msgs::msg::NavSatFix>(
                    topic, rclcpp::QoS(10));
            std::cout << "[Ros2TransportAdapter] GNSS publisher: " << topic
                      << " (domain " << reg.domain_id << ")" << std::endl;
        }
    }
    else if (stype == "imu")
    {
        std::string topic = reg.topic_override.empty()
            ? make_sensor_topic(stype, sname, "data")
            : reg.topic_override;
        if (info.imu_pubs.find(sname) == info.imu_pubs.end())
        {
            info.imu_pubs[sname] =
                info.node->create_publisher<sensor_msgs::msg::Imu>(
                    topic, rclcpp::QoS(10));
            std::cout << "[Ros2TransportAdapter] IMU publisher: " << topic
                      << " (domain " << reg.domain_id << ")" << std::endl;
        }
    }
    else if (stype == "diff_drive")
    {
        std::string odom_topic = reg.topic_override.empty()
            ? (sname.empty() ? "/odom" : "/" + sname + "/odom")
            : reg.topic_override;
        if (info.odom_pubs.find(sname) == info.odom_pubs.end())
        {
            info.odom_pubs[sname] =
                info.node->create_publisher<nav_msgs::msg::Odometry>(
                    odom_topic, rclcpp::QoS(10));
            std::cout << "[Ros2TransportAdapter] Odometry publisher: " << odom_topic
                      << " (domain " << reg.domain_id << ")" << std::endl;
        }
    }
    else
    {
        std::cerr << "[Ros2TransportAdapter] register_sensor: unknown sensor type '"
                  << stype << "'" << std::endl;
    }
}

void Ros2TransportAdapter::register_static_transforms(
    AgentId /*id*/, int domain_id,
    const std::vector<FrameTransform>& transforms)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = domain_nodes_.find(domain_id);
    if (it == domain_nodes_.end())
    {
        std::cerr << "[Ros2TransportAdapter] register_static_transforms: unknown domain "
                  << domain_id << std::endl;
        return;
    }
    auto& info = it->second;

    auto now = info.node->now();
    for (const auto& ft : transforms)
    {
        geometry_msgs::msg::TransformStamped ts;
        ts.header.stamp    = now;
        ts.header.frame_id = ft.parent_frame;
        ts.child_frame_id  = ft.child_frame;

        ts.transform.translation.x = ft.relative_pose.x;
        ts.transform.translation.y = ft.relative_pose.y;
        ts.transform.translation.z = ft.relative_pose.z;

        Eigen::Quaterniond q =
            Eigen::AngleAxisd(ft.relative_pose.yaw,   Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(ft.relative_pose.pitch, Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(ft.relative_pose.roll,  Eigen::Vector3d::UnitX());

        ts.transform.rotation.w = q.w();
        ts.transform.rotation.x = q.x();
        ts.transform.rotation.y = q.y();
        ts.transform.rotation.z = q.z();

        info.extra_static_tfs.push_back(ts);
    }

    // Переопубликовать static broadcaster с обновлёнными TF
    if (info.static_tf_broadcaster && !info.extra_static_tfs.empty())
    {
        info.static_tf_broadcaster->sendTransform(info.extra_static_tfs);
    }
}

void Ros2TransportAdapter::register_subscription(SubscriptionDesc desc)
{
    auto it = domain_nodes_.find(desc.domain_id);
    if (it == domain_nodes_.end())
    {
        std::cerr << "[Ros2TransportAdapter] register_subscription: unknown domain "
                  << desc.domain_id << std::endl;
        return;
    }
    auto& info = it->second;

    auto callback_fn = std::move(desc.callback);
    const std::string topic = desc.topic;

    // Поддерживаем nav_msgs/Path
    auto sub = info.node->create_subscription<nav_msgs::msg::Path>(
        topic, rclcpp::QoS(10),
        [callback_fn, topic](const nav_msgs::msg::Path::SharedPtr msg)
        {
            // Конвертируем nav_msgs/Path в JSON
            nlohmann::json j;
            nlohmann::json poses = nlohmann::json::array();
            for (const auto& pose_stamped : msg->poses)
            {
                nlohmann::json p;
                p["position"]["x"] = pose_stamped.pose.position.x;
                p["position"]["y"] = pose_stamped.pose.position.y;
                p["position"]["z"] = pose_stamped.pose.position.z;
                poses.push_back(p);
            }
            j["poses"] = poses;
            callback_fn(topic, j.dump());
        });

    info.subscriptions.push_back(sub);

    std::cout << "[Ros2TransportAdapter] Subscribed to Path " << desc.topic
              << " (domain " << desc.domain_id << ")" << std::endl;
}

void Ros2TransportAdapter::register_input_topic(InputTopicDesc desc)
{
    auto it = domain_nodes_.find(desc.domain_id);
    if (it == domain_nodes_.end())
    {
        std::cerr << "[Ros2TransportAdapter] register_input_topic: unknown domain "
                  << desc.domain_id << std::endl;
        return;
    }
    auto& info = it->second;

    auto callback_fn = std::move(desc.callback);
    auto sub = info.node->create_subscription<geometry_msgs::msg::Twist>(
        desc.topic, rclcpp::QoS(10),
        [callback_fn, plugin_type = desc.plugin_type](
            const geometry_msgs::msg::Twist::SharedPtr msg)
        {
            nlohmann::json j;
            if (plugin_type == "diff_drive")
            {
                j["linear_velocity"]  = msg->linear.x;
                j["angular_velocity"] = msg->angular.z;
            }
            else
            {
                j["linear_x"]  = msg->linear.x;
                j["linear_y"]  = msg->linear.y;
                j["linear_z"]  = msg->linear.z;
                j["angular_x"] = msg->angular.x;
                j["angular_y"] = msg->angular.y;
                j["angular_z"] = msg->angular.z;
            }
            callback_fn(j.dump());
        });

    info.subscriptions.push_back(sub);

    std::cout << "[Ros2TransportAdapter] Subscribed to " << desc.topic
              << " (domain " << desc.domain_id << ", plugin " << desc.plugin_type << ")"
              << std::endl;
}

void Ros2TransportAdapter::register_service(ServiceDesc desc)
{
    auto it = domain_nodes_.find(desc.domain_id);
    if (it == domain_nodes_.end())
    {
        std::cerr << "[Ros2TransportAdapter] register_service: unknown domain "
                  << desc.domain_id << std::endl;
        return;
    }
    auto& info = it->second;

    auto handler_fn = std::move(desc.handler);
    bool is_trigger = desc.is_trigger;

    if (is_trigger)
    {
        auto srv = info.node->create_service<std_srvs::srv::Trigger>(
            desc.service_name,
            [handler_fn](
                const std_srvs::srv::Trigger::Request::SharedPtr  /*req*/,
                std_srvs::srv::Trigger::Response::SharedPtr        response)
            {
                std::string result = handler_fn("{}");
                auto j = nlohmann::json::parse(result, nullptr, false);
                if (!j.is_discarded())
                {
                    response->success = j.value("success", false);
                    response->message = result;
                }
                else
                {
                    response->success = false;
                    response->message = result;
                }
            });
        info.services.push_back(srv);
    }
    else
    {
#ifdef S2_WITH_S2_MSGS
        auto srv = info.node->create_service<s2_msgs::srv::PluginCall>(
            desc.service_name,
            [handler_fn](
                const s2_msgs::srv::PluginCall::Request::SharedPtr  req,
                s2_msgs::srv::PluginCall::Response::SharedPtr        response)
            {
                std::string result = handler_fn(req->request_json);
                auto j = nlohmann::json::parse(result, nullptr, false);
                if (!j.is_discarded())
                {
                    response->success       = j.value("success", false);
                    response->response_json = result;
                }
                else
                {
                    response->success       = false;
                    response->response_json = result;
                }
            });
        info.services.push_back(srv);
#else
        auto srv = info.node->create_service<std_srvs::srv::Trigger>(
            desc.service_name,
            [handler_fn](
                const std_srvs::srv::Trigger::Request::SharedPtr,
                std_srvs::srv::Trigger::Response::SharedPtr response)
            {
                std::string result = handler_fn("{}");
                auto j = nlohmann::json::parse(result, nullptr, false);
                response->success = !j.is_discarded() && j.value("success", false);
                response->message = result;
            });
        info.services.push_back(srv);
#endif
    }

    std::cout << "[Ros2TransportAdapter] Service registered: " << desc.service_name
              << " (domain " << desc.domain_id << ")" << std::endl;
}

// ─── Публикация ─────────────────────────────────────────────────────────────

void Ros2TransportAdapter::publish_agent_frame(const AgentSensorFrame& frame)
{
    auto it = domain_nodes_.find(frame.domain_id);
    if (it == domain_nodes_.end()) return;
    auto& info = it->second;
    if (!info.initialized) return;

    auto now = info.node->now();

    // ── odom → base_link (dynamic TF, каждый кадр) ────────────────────────
    // Публикуем относительно начальной позы агента: робот стартует в (0,0,0) своего odom
    {
        geometry_msgs::msg::TransformStamped tf;
        tf.header.stamp    = now;
        tf.header.frame_id = "odom";
        tf.child_frame_id  = "base_link";

        const Pose3D& init = info.initial_pose;
        tf.transform.translation.x = frame.world_pose.x - init.x;
        tf.transform.translation.y = frame.world_pose.y - init.y;
        tf.transform.translation.z = frame.world_pose.z - init.z;

        // Абсолютная ориентация робота в мировом фрейме (odom world-fixed)
        Eigen::Quaterniond q_cur =
            Eigen::AngleAxisd(frame.world_pose.yaw,   Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(frame.world_pose.pitch, Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(frame.world_pose.roll,  Eigen::Vector3d::UnitX());

        tf.transform.rotation.w = q_cur.w();
        tf.transform.rotation.x = q_cur.x();
        tf.transform.rotation.y = q_cur.y();
        tf.transform.rotation.z = q_cur.z();

        info.tf_broadcaster->sendTransform(tf);
    }

    // ── Динамические TF из KinematicTree (revolute/prismatic джоинты) ────────
    for (const auto& ft : frame.dynamic_transforms)
    {
        geometry_msgs::msg::TransformStamped dyn_tf;
        dyn_tf.header.stamp    = now;
        dyn_tf.header.frame_id = ft.parent_frame;
        dyn_tf.child_frame_id  = ft.child_frame;

        dyn_tf.transform.translation.x = ft.relative_pose.x;
        dyn_tf.transform.translation.y = ft.relative_pose.y;
        dyn_tf.transform.translation.z = ft.relative_pose.z;

        Eigen::Quaterniond q_dyn =
            Eigen::AngleAxisd(ft.relative_pose.yaw,   Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(ft.relative_pose.pitch, Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(ft.relative_pose.roll,  Eigen::Vector3d::UnitX());

        dyn_tf.transform.rotation.w = q_dyn.w();
        dyn_tf.transform.rotation.x = q_dyn.x();
        dyn_tf.transform.rotation.y = q_dyn.y();
        dyn_tf.transform.rotation.z = q_dyn.z();

        info.tf_broadcaster->sendTransform(dyn_tf);
    }

    // ── Сенсоры — публикуем только те, что в frame.sensors ───────────────
    for (const auto& sensor : frame.sensors)
    {
        const std::string& sname = sensor.sensor_name;

        // ── GNSS ──────────────────────────────────────────────────────────
        if (sensor.sensor_type == "gnss" && sensor.gnss.has_value())
        {
            auto pub_it = info.gnss_pubs.find(sname);
            if (pub_it == info.gnss_pubs.end())
            {
                std::cerr << "[Ros2TransportAdapter] No GNSS publisher for name='"
                          << sname << "'" << std::endl;
                continue;
            }

            const auto& gd = sensor.gnss.value();
            sensor_msgs::msg::NavSatFix fix;
            fix.header.stamp    = now;
            fix.header.frame_id = sname.empty() ? "gnss_link" : sname + "_gnss_link";

            fix.latitude  = gd.lat;
            fix.longitude = gd.lon;
            fix.altitude  = gd.alt;

            fix.status.status  = sensor_msgs::msg::NavSatStatus::STATUS_FIX;
            fix.status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;

            fix.position_covariance_type =
                sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
            double acc2 = gd.accuracy * gd.accuracy;
            fix.position_covariance[0] = acc2;
            fix.position_covariance[4] = acc2;
            fix.position_covariance[8] = acc2;

            pub_it->second->publish(fix);
        }

        // ── IMU ───────────────────────────────────────────────────────────
        else if (sensor.sensor_type == "imu" && sensor.imu.has_value())
        {
            auto pub_it = info.imu_pubs.find(sname);
            if (pub_it == info.imu_pubs.end())
            {
                std::cerr << "[Ros2TransportAdapter] No IMU publisher for name='"
                          << sname << "'" << std::endl;
                continue;
            }

            const auto& id = sensor.imu.value();
            sensor_msgs::msg::Imu imu_msg;
            imu_msg.header.stamp    = now;
            imu_msg.header.frame_id = sname.empty() ? "imu_link" : sname + "_imu_link";

            imu_msg.angular_velocity.x = id.gyro_x;
            imu_msg.angular_velocity.y = id.gyro_y;
            imu_msg.angular_velocity.z = id.gyro_z;

            imu_msg.linear_acceleration.x = id.accel_x;
            imu_msg.linear_acceleration.y = id.accel_y;
            imu_msg.linear_acceleration.z = id.accel_z;

            Eigen::Quaterniond q{
                Eigen::AngleAxisd(id.yaw, Eigen::Vector3d::UnitZ())};
            imu_msg.orientation.w = q.w();
            imu_msg.orientation.x = q.x();
            imu_msg.orientation.y = q.y();
            imu_msg.orientation.z = q.z();

            imu_msg.orientation_covariance[0]         = -1.0;
            imu_msg.angular_velocity_covariance[0]    = -1.0;
            imu_msg.linear_acceleration_covariance[0] = -1.0;

            pub_it->second->publish(imu_msg);
        }

        // ── DiffDrive (Odometry) ──────────────────────────────────────────
        else if (sensor.sensor_type == "diff_drive" && sensor.diff_drive.has_value())
        {
            auto pub_it = info.odom_pubs.find(sname);
            if (pub_it == info.odom_pubs.end())
            {
                std::cerr << "[Ros2TransportAdapter] No Odom publisher for name='"
                          << sname << "'" << std::endl;
                continue;
            }

            nav_msgs::msg::Odometry odom;
            odom.header.stamp    = now;
            odom.header.frame_id = "odom";
            odom.child_frame_id  = odom_child_frame(sname);

            odom.pose.pose.position.x = frame.world_pose.x;
            odom.pose.pose.position.y = frame.world_pose.y;
            odom.pose.pose.position.z = frame.world_pose.z;

            Eigen::Quaterniond q{
                Eigen::AngleAxisd(frame.world_pose.yaw, Eigen::Vector3d::UnitZ())};
            odom.pose.pose.orientation.w = q.w();
            odom.pose.pose.orientation.x = q.x();
            odom.pose.pose.orientation.y = q.y();
            odom.pose.pose.orientation.z = q.z();

            odom.twist.twist.linear.x  = frame.world_velocity.linear.x();
            odom.twist.twist.linear.y  = frame.world_velocity.linear.y();
            odom.twist.twist.angular.z = frame.world_velocity.angular.z();

            pub_it->second->publish(odom);
        }
    }
}

void Ros2TransportAdapter::emit_event(const TransportEvent& event)
{
    auto it = domain_nodes_.find(event.domain_id);
    if (it == domain_nodes_.end()) return;
    auto& info = it->second;

    auto pub_it = info.event_pubs.find(event.topic);
    if (pub_it == info.event_pubs.end())
    {
        auto pub = info.node->create_publisher<std_msgs::msg::String>(
            event.topic, rclcpp::QoS(10));
        info.event_pubs[event.topic] = pub;
        pub_it = info.event_pubs.find(event.topic);
    }

    std_msgs::msg::String msg;
    msg.data = event.payload_json;
    pub_it->second->publish(msg);
}

// ─── Вспомогательные методы ────────────────────────────────────────────────

Ros2TransportAdapter::NodeInfo& Ros2TransportAdapter::get_or_create_node(int domain_id)
{
    auto it = domain_nodes_.find(domain_id);
    if (it != domain_nodes_.end())
    {
        return it->second;
    }

    NodeInfo info;

    info.context = std::make_shared<rclcpp::Context>();
    rclcpp::InitOptions init_options;
    init_options.set_domain_id(static_cast<size_t>(domain_id));
    info.context->init(0, nullptr, init_options);

    rclcpp::NodeOptions node_opts;
    node_opts.context(info.context);
    info.node = std::make_shared<rclcpp::Node>(
        "s2_sim_domain_" + std::to_string(domain_id), node_opts);

    rclcpp::ExecutorOptions exec_opts;
    exec_opts.context = info.context;
    info.executor = std::make_unique<rclcpp::executors::SingleThreadedExecutor>(exec_opts);
    info.executor->add_node(info.node);

    std::cout << "[Ros2TransportAdapter] Created isolated node for domain "
              << domain_id << std::endl;

    domain_nodes_[domain_id] = std::move(info);
    return domain_nodes_[domain_id];
}

geometry_msgs::msg::TransformStamped
Ros2TransportAdapter::compute_earth_map_tf(
    const GeoOrigin& origin,
    const Pose3D& initial_pose,
    const std::shared_ptr<rclcpp::Node>& node) const
{
    geometry_msgs::msg::TransformStamped tf;
    tf.header.stamp    = node->now();
    tf.header.frame_id = "earth";
    tf.child_frame_id  = "map";

    const double a  = 6378137.0;
    const double e2 = 0.00669437999014;
    const double lat_r = origin.lat * M_PI / 180.0;
    const double lon_r = origin.lon * M_PI / 180.0;
    const double sin_lat = std::sin(lat_r);
    const double cos_lat = std::cos(lat_r);
    const double sin_lon = std::sin(lon_r);
    const double cos_lon = std::cos(lon_r);
    const double N = a / std::sqrt(1.0 - e2 * sin_lat * sin_lat);

    // ECEF позиция geo_origin
    Eigen::Vector3d ecef_origin(
        (N + origin.alt) * cos_lat * cos_lon,
        (N + origin.alt) * cos_lat * sin_lon,
        (N * (1.0 - e2) + origin.alt) * sin_lat);

    // Матрица ECEF→ENU при geo_origin; её транспонирование = ENU→ECEF
    Eigen::Matrix3d R_enu;
    R_enu << -sin_lon,             cos_lon,           0.0,
             -sin_lat * cos_lon,  -sin_lat * sin_lon,  cos_lat,
              cos_lat * cos_lon,   cos_lat * sin_lon,  sin_lat;

    // Смещаем ECEF-позицию origin на начальную позу агента в ENU (x=East, y=North, z=Up)
    Eigen::Vector3d enu_offset(initial_pose.x, initial_pose.y, initial_pose.z);
    Eigen::Vector3d ecef_pos = ecef_origin + R_enu.transpose() * enu_offset;

    tf.transform.translation.x = ecef_pos.x();
    tf.transform.translation.y = ecef_pos.y();
    tf.transform.translation.z = ecef_pos.z();

    // Ориентация: ENU-базис в ECEF — одинакова для всех роботов (небольшие смещения
    // в пределах сотен метров не меняют ориентацию значимо)
    Eigen::Quaterniond q(R_enu);
    tf.transform.rotation.w = q.w();
    tf.transform.rotation.x = q.x();
    tf.transform.rotation.y = q.y();
    tf.transform.rotation.z = q.z();

    return tf;
}

void Ros2TransportAdapter::setup_static_tf(NodeInfo& info)
{
    std::vector<geometry_msgs::msg::TransformStamped> static_tfs;

    if (geo_origin_set_)
    {
        info.earth_map_tf = compute_earth_map_tf(geo_origin_, info.initial_pose, info.node);
        static_tfs.push_back(info.earth_map_tf);

        NodeInfo* info_ptr = &info;
        info.earth_map_timer = info.node->create_wall_timer(
            std::chrono::seconds(1),
            [info_ptr]()
            {
                info_ptr->earth_map_tf.header.stamp = info_ptr->node->now();
                std::vector<geometry_msgs::msg::TransformStamped> tfs{info_ptr->earth_map_tf};
                info_ptr->static_tf_broadcaster->sendTransform(tfs);
            });
    }

    // map → odom: identity
    {
        geometry_msgs::msg::TransformStamped map_odom;
        map_odom.header.stamp    = info.node->now();
        map_odom.header.frame_id = "map";
        map_odom.child_frame_id  = "odom";
        map_odom.transform.rotation.w = 1.0;
        static_tfs.push_back(map_odom);
    }

    if (!static_tfs.empty())
    {
        info.static_tf_broadcaster->sendTransform(static_tfs);
    }
}

} // namespace s2

#endif // S2_WITH_ROS2
