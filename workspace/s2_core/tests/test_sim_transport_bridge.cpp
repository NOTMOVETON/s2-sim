#include <gtest/gtest.h>

#include <s2/sim_engine.hpp>
#include <s2/sim_transport_bridge.hpp>
#include <s2/transport_adapter.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/gnss.hpp>
#include <s2/plugins/imu.hpp>

#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#include <map>

using namespace s2;

// ─── Mock-адаптер для тестирования без ROS2 ──────────────────────────────────

class MockTransportAdapter : public ITransportAdapter
{
public:
    void start() override { started = true; }
    void stop()  override { stopped = true; }

    void set_geo_origin(const GeoOrigin& o) override { geo_origin = o; }

    void register_agent(AgentId id, int domain, const std::string& name,
                        const Pose3D& /*initial_pose*/) override
    {
        registered_agents.push_back({id, domain, name});
    }

    void register_sensor(SensorRegistration reg) override
    {
        registered_sensors.push_back(reg);
    }

    void register_input_topic(InputTopicDesc desc) override
    {
        registered_topics.push_back(desc.topic);
        input_callbacks[desc.topic] = desc.callback;
    }

    void register_service(ServiceDesc desc) override
    {
        registered_services.push_back(desc.service_name);
    }

    void register_static_transforms(AgentId id, int domain,
                                    const std::vector<FrameTransform>& transforms) override
    {
        for (const auto& ft : transforms)
            registered_static_tfs.push_back({id, domain, ft});
    }

    void publish_agent_frame(const AgentSensorFrame& frame) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        published_frames.push_back(frame);
    }

    void emit_event(const TransportEvent& event) override
    {
        emitted_events.push_back(event);
    }

    void register_subscription(SubscriptionDesc desc) override
    {
        registered_subscriptions.push_back(desc.topic);
    }

    // Симулировать получение сообщения на топик
    void trigger_topic(const std::string& topic, const std::string& json)
    {
        auto it = input_callbacks.find(topic);
        if (it != input_callbacks.end()) it->second(json);
    }

    // ── Вспомогательные методы поиска в frames ────────────────────────────

    // Найти SensorOutput заданного типа и имени в последнем опубликованном кадре агента
    const SensorOutput* find_sensor(AgentId agent_id,
                                    const std::string& sensor_type,
                                    const std::string& sensor_name = "") const
    {
        for (auto it = published_frames.rbegin(); it != published_frames.rend(); ++it)
        {
            if (it->agent_id != agent_id) continue;
            for (const auto& s : it->sensors)
            {
                if (s.sensor_type == sensor_type && s.sensor_name == sensor_name)
                    return &s;
            }
        }
        return nullptr;
    }

    // Подсчитать кадры агента с данными сенсора заданного типа
    int count_sensor_frames(AgentId agent_id, const std::string& sensor_type) const
    {
        int count = 0;
        for (const auto& f : published_frames)
        {
            if (f.agent_id != agent_id) continue;
            for (const auto& s : f.sensors)
                if (s.sensor_type == sensor_type) ++count;
        }
        return count;
    }

    // Данные
    bool started{false};
    bool stopped{false};
    GeoOrigin geo_origin;

    struct AgentReg { AgentId id; int domain; std::string name; };
    struct StaticTfReg { AgentId id; int domain; FrameTransform ft; };

    std::vector<AgentReg>          registered_agents;
    std::vector<SensorRegistration> registered_sensors;
    std::vector<std::string>       registered_topics;
    std::vector<std::string>       registered_services;
    std::vector<std::string>       registered_subscriptions;
    std::vector<StaticTfReg>       registered_static_tfs;

    std::mutex mtx;
    std::vector<AgentSensorFrame> published_frames;
    std::vector<TransportEvent>   emitted_events;

    std::map<std::string, std::function<void(const std::string&)>> input_callbacks;
};

// ─── Вспомогательные функции ──────────────────────────────────────────────────

static s2::SimEngine make_engine(double transport_rate = 100.0)
{
    return s2::SimEngine{{.update_rate = 100.0, .viz_rate = 30.0,
                          .transport_rate = transport_rate}};
}

static s2::Agent make_robot(AgentId id, const std::string& name, int domain = 0)
{
    Agent agent;
    agent.id = id;
    agent.name = name;
    agent.domain_id = domain;

    agent.plugins.push_back(std::make_unique<plugins::DiffDrivePlugin>());
    agent.plugins.push_back(std::make_unique<plugins::GnssPlugin>());
    agent.plugins.push_back(std::make_unique<plugins::ImuPlugin>());
    return agent;
}

// ─── Базовые тесты ───────────────────────────────────────────────────────────

TEST(SimTransportBridgeTest, InitRegistersAgents)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    world.add_agent(make_robot(1, "robot_1", 1));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);

    GeoOrigin origin{55.75, 37.62, 150.0};
    bridge.init(origin);

    EXPECT_EQ(mock->registered_agents.size(), 2u);
    EXPECT_EQ(mock->registered_agents[0].id, 0);
    EXPECT_EQ(mock->registered_agents[1].id, 1);
    EXPECT_EQ(mock->registered_agents[0].name, "robot_0");
    EXPECT_DOUBLE_EQ(mock->geo_origin.lat, 55.75);
}

TEST(SimTransportBridgeTest, InitRegistersSensors)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // robot_0 имеет diff_drive + gnss + imu → 3 сенсора
    EXPECT_EQ(mock->registered_sensors.size(), 3u);

    // Проверяем типы
    bool has_gnss = false, has_imu = false, has_dd = false;
    for (const auto& s : mock->registered_sensors)
    {
        if (s.sensor_type == "gnss")       has_gnss = true;
        if (s.sensor_type == "imu")        has_imu  = true;
        if (s.sensor_type == "diff_drive") has_dd   = true;
        EXPECT_EQ(s.agent_id, 0);
        EXPECT_EQ(s.sensor_name, "");  // безымянные
    }
    EXPECT_TRUE(has_gnss);
    EXPECT_TRUE(has_imu);
    EXPECT_TRUE(has_dd);
}

TEST(SimTransportBridgeTest, InitRegistersCmdVelTopic)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // DiffDrivePlugin объявляет /cmd_vel
    EXPECT_FALSE(mock->registered_topics.empty());
    EXPECT_EQ(mock->registered_topics[0], "/cmd_vel");
}

TEST(SimTransportBridgeTest, StartStop)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    bridge.start();
    EXPECT_TRUE(mock->started);

    bridge.stop();
    EXPECT_TRUE(mock->stopped);
}

TEST(SimTransportBridgeTest, PostTickPublishesFrames)
{
    auto engine_val = make_engine();
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    agent.world_pose.x = 1.5;
    agent.world_pose.y = 2.5;
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    engine_val.step(1);
    bridge.on_post_tick(engine_val.world(), engine_val.sim_time());

    ASSERT_FALSE(mock->published_frames.empty());
    const auto& frame = mock->published_frames[0];
    EXPECT_EQ(frame.agent_id, 0);
    EXPECT_DOUBLE_EQ(frame.world_pose.x, 1.5);
    EXPECT_DOUBLE_EQ(frame.world_pose.y, 2.5);
}

TEST(SimTransportBridgeTest, PostTickIncludesSensorData)
{
    auto engine_val = make_engine();
    SimWorld world;

    GeoOrigin origin{55.75, 37.62, 150.0};

    Agent agent = make_robot(0, "robot_0", 0);
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    for (auto& a : engine_val.world().agents()) {
        for (auto& plugin : a.plugins) {
            if (plugin->type() == "gnss") {
                auto* gnss = dynamic_cast<plugins::GnssPlugin*>(plugin.get());
                if (gnss) gnss->set_geo_origin(origin);
            }
        }
    }

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init(origin);

    // IMU по умолчанию 100Hz = каждый тик
    // GNSS по умолчанию 10Hz → каждые 10 тиков
    engine_val.step(10);

    // Вызываем on_post_tick вручную после тиков
    bridge.on_post_tick(engine_val.world(), engine_val.sim_time());

    ASSERT_FALSE(mock->published_frames.empty());

    // GNSS должен быть опубликован хотя бы раз за 10 тиков
    EXPECT_NE(mock->find_sensor(0, "gnss"), nullptr);

    // IMU должен быть опубликован
    EXPECT_NE(mock->find_sensor(0, "imu"), nullptr);

    auto* gnss_out = mock->find_sensor(0, "gnss");
    if (gnss_out && gnss_out->gnss)
    {
        EXPECT_NEAR(gnss_out->gnss->lat, 55.75, 0.01);
        EXPECT_NEAR(gnss_out->gnss->lon, 37.62, 0.01);
    }

    auto* imu_out = mock->find_sensor(0, "imu");
    if (imu_out && imu_out->imu)
    {
        EXPECT_NEAR(imu_out->imu->accel_z, 9.81, 0.01);
    }
}

TEST(SimTransportBridgeTest, TopicCallbackRoutesToPlugin)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    mock->trigger_topic("/cmd_vel",
        R"({"linear_velocity": 1.0, "angular_velocity": 0.5})");

    engine_val.step(1);

    const auto* a = engine_val.world().get_agent(0);
    ASSERT_NE(a, nullptr);
    EXPECT_NEAR(a->world_velocity.linear.x(), 1.0, 0.01);
    EXPECT_NEAR(a->world_velocity.angular.z(), 0.5, 0.01);
}

TEST(SimTransportBridgeTest, PostTickCallbackRegisteredInEngine)
{
    auto engine_val = make_engine();
    SimWorld world;
    world.add_agent(make_robot(0, "robot_0", 0));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // transport_rate=100 при sim=100 → каждый тик
    engine_val.step(5);

    EXPECT_FALSE(mock->published_frames.empty());
}

TEST(SimTransportBridgeTest, NoSensorDataIfPluginAbsent)
{
    auto engine_val = make_engine();
    SimWorld world;

    Agent agent;
    agent.id = 0;
    agent.name = "bare_robot";
    agent.domain_id = 0;
    agent.plugins.push_back(std::make_unique<plugins::DiffDrivePlugin>());
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    engine_val.step(1);
    bridge.on_post_tick(engine_val.world(), engine_val.sim_time());

    ASSERT_FALSE(mock->published_frames.empty());

    // GNSS и IMU отсутствуют
    EXPECT_EQ(mock->find_sensor(0, "gnss"), nullptr);
    EXPECT_EQ(mock->find_sensor(0, "imu"),  nullptr);
    // DiffDrive есть
    EXPECT_NE(mock->find_sensor(0, "diff_drive"), nullptr);
}

// ─── Тесты частоты публикации (per-sensor rate) ───────────────────────────────

TEST(SimTransportBridgeTest, GnssPublishesAtOwnRate)
{
    // transport_rate=100, GNSS по умолчанию 10Hz → должен публиковать ~каждые 10 тиков
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // 100 тиков = 1 секунда при 100Hz
    for (int i = 0; i < 100; ++i)
    {
        engine_val.step(1);
    }

    // GNSS при 10Hz за 1 секунду → ~10 публикаций
    int gnss_count = mock->count_sensor_frames(0, "gnss");
    EXPECT_GE(gnss_count, 8);   // допуск ±20%
    EXPECT_LE(gnss_count, 12);
}

TEST(SimTransportBridgeTest, ImuPublishesAtOwnRate)
{
    // IMU по умолчанию 100Hz = каждый тик при transport_rate=100
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // 50 тиков
    for (int i = 0; i < 50; ++i)
    {
        engine_val.step(1);
    }

    // IMU при 100Hz за 50 тиков → должно быть ~50 публикаций
    int imu_count = mock->count_sensor_frames(0, "imu");
    EXPECT_GE(imu_count, 40);
    EXPECT_LE(imu_count, 55);
}

TEST(SimTransportBridgeTest, SeqPreventsDoublPublish)
{
    // Если transport_rate > sensor_rate, один seq не должен публиковаться дважды
    auto engine_val = make_engine(100.0);  // bridge каждый тик
    SimWorld world;

    // Только IMU, без GNSS и DiffDrive
    Agent agent;
    agent.id = 0;
    agent.name = "imu_only";
    agent.domain_id = 0;
    auto imu = std::make_unique<plugins::ImuPlugin>();
    agent.plugins.push_back(std::move(imu));
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // 5 тиков
    engine_val.step(5);

    // Все IMU кадры должны иметь разные seq
    std::vector<uint64_t> seqs;
    for (const auto& f : mock->published_frames)
    {
        if (f.agent_id != 0) continue;
        for (const auto& s : f.sensors)
        {
            if (s.sensor_type == "imu" && s.imu)
                seqs.push_back(s.imu->seq);
        }
    }
    // Проверяем, что все seq уникальны (нет дубликатов)
    std::sort(seqs.begin(), seqs.end());
    auto last = std::unique(seqs.begin(), seqs.end());
    EXPECT_EQ(last, seqs.end()) << "IMU data published multiple times with same seq";
}

// ─── Тесты именованных сенсоров ───────────────────────────────────────────────

TEST(SimTransportBridgeTest, NamedSensorRegistered)
{
    auto engine_val = make_engine();
    SimWorld world;

    Agent agent;
    agent.id = 0;
    agent.name = "multi_gnss_robot";
    agent.domain_id = 0;

    // GNSS с именем "front"
    auto gnss_front = std::make_unique<plugins::GnssPlugin>();
    gnss_front->set_sensor_name("front");
    agent.plugins.push_back(std::move(gnss_front));

    // GNSS с именем "rear"
    // (с текущей реализацией SharedState оба пишут в GnssData,
    //  второй перезаписывает первый — это ограничение SharedState)
    auto gnss_rear = std::make_unique<plugins::GnssPlugin>();
    gnss_rear->set_sensor_name("rear");
    agent.plugins.push_back(std::move(gnss_rear));

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // Оба GNSS должны быть зарегистрированы с разными именами
    bool has_front = false, has_rear = false;
    for (const auto& s : mock->registered_sensors)
    {
        if (s.sensor_type == "gnss" && s.sensor_name == "front") has_front = true;
        if (s.sensor_type == "gnss" && s.sensor_name == "rear")  has_rear  = true;
    }
    EXPECT_TRUE(has_front) << "front GNSS не зарегистрирован";
    EXPECT_TRUE(has_rear)  << "rear GNSS не зарегистрирован";
}

TEST(SimTransportBridgeTest, NamedSensorInSensorOutput)
{
    auto engine_val = make_engine();
    SimWorld world;

    Agent agent;
    agent.id = 0;
    agent.name = "robot";
    agent.domain_id = 0;

    auto imu = std::make_unique<plugins::ImuPlugin>();
    imu->set_sensor_name("chassis");
    agent.plugins.push_back(std::move(imu));

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    engine_val.step(1);
    bridge.on_post_tick(engine_val.world(), engine_val.sim_time());

    // Должны найти IMU с именем "chassis"
    auto* out = mock->find_sensor(0, "imu", "chassis");
    EXPECT_NE(out, nullptr) << "IMU с именем 'chassis' не найден в frame";
    if (out)
    {
        EXPECT_EQ(out->sensor_name, "chassis");
        EXPECT_TRUE(out->imu.has_value());
    }
}

TEST(SimTransportBridgeTest, SensorNameNotMixedUp)
{
    // Проверяем, что unnamed сенсор не путается с named
    auto engine_val = make_engine();
    SimWorld world;

    Agent agent;
    agent.id = 0;
    agent.name = "robot";
    agent.domain_id = 0;

    // IMU без имени
    agent.plugins.push_back(std::make_unique<plugins::ImuPlugin>());
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    engine_val.step(1);
    bridge.on_post_tick(engine_val.world(), engine_val.sim_time());

    // sensor_name должно быть пустой строкой
    auto* out = mock->find_sensor(0, "imu", "");
    EXPECT_NE(out, nullptr);
    if (out)
    {
        EXPECT_EQ(out->sensor_name, "");
    }

    // Поиск по несуществующему имени — должен вернуть nullptr
    EXPECT_EQ(mock->find_sensor(0, "imu", "front"), nullptr);
}

// ─── Тесты SceneLoader integration ───────────────────────────────────────────

TEST(SimTransportBridgeTest, TransportRateAffectsPublishFrequency)
{
    // transport_rate=10 → bridge вызывается каждые 10 тиков (при sim=100)
    auto engine_val = make_engine(10.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // 10 тиков при transport_rate=10 → должен выстрелить 1 раз
    engine_val.step(10);

    // Кадры должны быть (хотя бы 1)
    EXPECT_GE(mock->published_frames.size(), 1u);
    // Но не более ~2 (не каждый тик)
    EXPECT_LE(mock->published_frames.size(), 2u);
}

// ─── Тесты KinematicTree integration ─────────────────────────────────────────

TEST(SimTransportBridgeTest, StaticTransformsFromKinematicTree)
{
    // Агент с двумя fixed-звеньями → 2 статических TF при init()
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    agent.kinematic_tree = std::make_unique<KinematicTree>();

    Link arm1;
    arm1.name = "arm1_link"; arm1.parent = "base_link";
    arm1.origin = Pose3D{0.5, 0.0, 0.0, 0.0, 0.0, 0.0};
    arm1.joint.type = JointType::FIXED;
    agent.kinematic_tree->add_link(arm1);

    Link camera;
    camera.name = "camera_link"; camera.parent = "arm1_link";
    camera.origin = Pose3D{0.2, 0.0, 0.1, 0.0, 0.0, 0.0};
    camera.joint.type = JointType::FIXED;
    agent.kinematic_tree->add_link(camera);

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // 2 fixed links → 2 static TF
    EXPECT_EQ(mock->registered_static_tfs.size(), 2u);

    bool found_arm1 = false, found_camera = false;
    for (const auto& reg : mock->registered_static_tfs)
    {
        if (reg.ft.child_frame == "arm1_link"   && reg.ft.parent_frame == "base_link")  found_arm1   = true;
        if (reg.ft.child_frame == "camera_link" && reg.ft.parent_frame == "arm1_link")  found_camera = true;
    }
    EXPECT_TRUE(found_arm1);
    EXPECT_TRUE(found_camera);
}

TEST(SimTransportBridgeTest, DynamicTransformsInFrame)
{
    // Агент с revolute-джоинтом → dynamic_transforms в каждом кадре
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    agent.kinematic_tree = std::make_unique<KinematicTree>();

    Link arm;
    arm.name = "arm_link"; arm.parent = "base_link";
    arm.origin = Pose3D{0.3, 0.0, 0.0, 0.0, 0.0, 0.0};
    arm.joint.type  = JointType::REVOLUTE;
    arm.joint.axis  = Vec3{0.0, 0.0, 1.0};
    arm.joint.value = 0.5;
    arm.joint.min   = -M_PI;
    arm.joint.max   =  M_PI;
    agent.kinematic_tree->add_link(arm);

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // Revolute → не попадает в static
    EXPECT_EQ(mock->registered_static_tfs.size(), 0u);

    engine_val.step(1);

    ASSERT_FALSE(mock->published_frames.empty());
    const auto& frame = mock->published_frames.front();
    ASSERT_EQ(frame.dynamic_transforms.size(), 1u);
    EXPECT_EQ(frame.dynamic_transforms[0].child_frame, "arm_link");
    EXPECT_EQ(frame.dynamic_transforms[0].parent_frame, "base_link");
}

TEST(SimTransportBridgeTest, MountFrameFromPlugin)
{
    // Плагин с set_mount_pose → static TF при init()
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);

    // make_robot уже добавил GnssPlugin — задаём ему монтажную позу
    for (auto& p : agent.plugins)
    {
        if (p->type() == "gnss")
        {
            p->set_mount_pose(Pose3D{0.1, 0.0, 0.5, 0.0, 0.0, 0.0});
            break;
        }
    }

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    // Монтажная поза плагина → хотя бы 1 static TF с parent == "base_link"
    EXPECT_GE(mock->registered_static_tfs.size(), 1u);

    bool found = false;
    for (const auto& reg : mock->registered_static_tfs)
    {
        if (reg.ft.parent_frame == "base_link") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(SimTransportBridgeTest, NoStaticTransformsWithoutTree)
{
    // Обычный агент без kinematic_tree и без mount_pose → 0 static TF
    auto engine_val = make_engine(100.0);
    SimWorld world;

    Agent agent = make_robot(0, "robot_0", 0);
    // kinematic_tree == nullptr (по умолчанию)
    // sensors без mount_pose

    world.add_agent(std::move(agent));
    engine_val.load_world(std::move(world));

    auto mock = std::make_shared<MockTransportAdapter>();
    SimTransportBridge bridge(&engine_val, mock);
    bridge.init({});

    EXPECT_EQ(mock->registered_static_tfs.size(), 0u);
}
