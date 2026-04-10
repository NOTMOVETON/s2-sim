/**
 * @file sim_transport_bridge.cpp
 * Реализация SimTransportBridge.
 */

#include <s2/sim_transport_bridge.hpp>
#include <s2/world.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>
#include <s2/sensor_data.hpp>

#include <iostream>

namespace s2
{

// Типы плагинов, публикующих сенсорные данные
bool SimTransportBridge::is_sensor_plugin(const std::string& plugin_type)
{
    return plugin_type == "gnss"
        || plugin_type == "imu"
        || plugin_type == "diff_drive";
}

SimTransportBridge::SimTransportBridge(SimEngine* engine,
                                       std::shared_ptr<ITransportAdapter> adapter)
    : engine_(engine)
    , adapter_(std::move(adapter))
{
}

void SimTransportBridge::init(const GeoOrigin& geo_origin)
{
    adapter_->set_geo_origin(geo_origin);

    for (const auto& agent : engine_->world().agents())
    {
        adapter_->register_agent(agent.id, agent.domain_id, agent.name, agent.world_pose);

        // Собрать статические трансформы: fixed-джоинты дерева + mount_frame плагинов
        std::vector<FrameTransform> static_tfs;

        if (agent.kinematic_tree)
        {
            std::vector<KinematicFrameTransform> kin_static, kin_dyn;
            agent.kinematic_tree->collect_transforms(kin_static, kin_dyn);
            for (const auto& kft : kin_static)
            {
                static_tfs.push_back({kft.parent_frame, kft.child_frame, kft.relative_pose});
            }
        }

        // Инициализируем плагины (запоминают начальное состояние агента)
        for (const auto& plugin : agent.plugins)
        {
            plugin->initialize(const_cast<Agent&>(agent));
        }

        for (const auto& plugin : agent.plugins)
        {
            auto mf = plugin->mount_frame();
            if (mf.has_value())
                static_tfs.push_back(mf.value());
        }

        if (!static_tfs.empty())
            adapter_->register_static_transforms(agent.id, agent.domain_id, static_tfs);

        for (const auto& plugin : agent.plugins)
        {
            // Регистрируем сенсоры (адаптер создаёт publisher'ы)
            if (is_sensor_plugin(plugin->type()))
            {
                SensorRegistration reg;
                reg.agent_id      = agent.id;
                reg.domain_id     = agent.domain_id;
                reg.sensor_type   = plugin->type();
                reg.sensor_name   = plugin->sensor_name();
                reg.topic_override = plugin->output_topic();
                adapter_->register_sensor(reg);

                // Инициализируем счётчик seq нулём
                last_published_seq_[{agent.id, plugin->type(), plugin->sensor_name()}] = 0;
            }

            // Регистрируем входные топики команд
            for (const auto& topic : plugin->command_topics())
            {
                InputTopicDesc desc;
                desc.topic       = topic;
                desc.plugin_type = plugin->type();
                desc.agent_id    = agent.id;
                desc.domain_id   = agent.domain_id;

                AgentId captured_id       = agent.id;
                std::string captured_type = plugin->type();
                SimEngine*  eng           = engine_;
                desc.callback = [eng, captured_id, captured_type](const std::string& json)
                {
                    eng->handle_plugin_input(captured_id, captured_type, json);
                };

                adapter_->register_input_topic(std::move(desc));
            }

            // Регистрируем подписки на внешние топики данных (subscribe_topics)
            for (const auto& topic : plugin->subscribe_topics())
            {
                SubscriptionDesc desc;
                desc.topic       = topic;
                desc.msg_type    = "nav_msgs/Path";  // единственный поддерживаемый тип
                desc.plugin_type = plugin->type();
                desc.agent_id    = agent.id;
                desc.domain_id   = agent.domain_id;

                plugins::IAgentPlugin* raw_plugin = plugin.get();
                desc.callback = [raw_plugin](const std::string& t, const std::string& json)
                {
                    raw_plugin->handle_subscription(t, json);
                };

                adapter_->register_subscription(std::move(desc));
            }

            // Регистрируем сервисы
            for (const auto& svc_name : plugin->service_names())
            {
                ServiceDesc desc;
                desc.service_name = svc_name;
                desc.plugin_type  = plugin->type();
                desc.agent_id     = agent.id;
                desc.domain_id    = agent.domain_id;
                desc.is_trigger   = true;

                plugins::IAgentPlugin* raw_plugin = plugin.get();
                std::string captured_svc = svc_name;
                desc.handler = [raw_plugin, captured_svc](const std::string& req_json)
                {
                    return raw_plugin->handle_service(captured_svc, req_json);
                };

                adapter_->register_service(std::move(desc));
            }
        }
    }

    // Устанавливаем post-tick callback в SimEngine
    engine_->set_post_tick_callback(
        [this](const SimWorld& world, double sim_time)
        {
            on_post_tick(world, sim_time);
        });

    std::cout << "[SimTransportBridge] Initialized, agents: "
              << engine_->world().agents().size() << std::endl;
}

void SimTransportBridge::start()
{
    adapter_->start();
}

void SimTransportBridge::stop()
{
    adapter_->stop();
}

void SimTransportBridge::on_post_tick(const SimWorld& world, double sim_time)
{
    for (const auto& agent : world.agents())
    {
        AgentSensorFrame frame;
        frame.agent_id       = agent.id;
        frame.domain_id      = agent.domain_id;
        frame.sim_time       = sim_time;
        frame.world_pose     = agent.world_pose;
        frame.world_velocity = agent.world_velocity;

        // Добавляем сенсор в frame только если его seq изменился
        for (const auto& plugin : agent.plugins)
        {
            if (!is_sensor_plugin(plugin->type()))
                continue;

            SensorKey key{agent.id, plugin->type(), plugin->sensor_name()};

            const std::string& ptype = plugin->type();
            const std::string& pname = plugin->sensor_name();

            SensorOutput out;
            out.sensor_type = ptype;
            out.sensor_name = pname;

            bool has_new_data = false;

            if (ptype == "gnss")
            {
                auto* data = agent.state.get<GnssData>();
                auto it = last_published_seq_.find(key);
                if (data && (it == last_published_seq_.end() || data->seq > it->second))
                {
                    out.gnss = *data;  // копируем — не храним указатель на SharedState
                    has_new_data = true;
                    last_published_seq_[key] = data->seq;
                }
            }
            else if (ptype == "imu")
            {
                auto* data = agent.state.get<ImuData>();
                auto it = last_published_seq_.find(key);
                if (data && (it == last_published_seq_.end() || data->seq > it->second))
                {
                    out.imu = *data;
                    has_new_data = true;
                    last_published_seq_[key] = data->seq;
                }
            }
            else if (ptype == "diff_drive")
            {
                auto* data = agent.state.get<DiffDriveData>();
                auto it = last_published_seq_.find(key);
                if (data && (it == last_published_seq_.end() || data->seq > it->second))
                {
                    out.diff_drive = *data;
                    has_new_data = true;
                    last_published_seq_[key] = data->seq;
                }
            }

            if (has_new_data)
            {
                frame.sensors.push_back(out);
            }
        }

        // Динамические трансформы из KinematicTree (revolute/prismatic джоинты)
        if (agent.kinematic_tree)
        {
            std::vector<KinematicFrameTransform> kin_static, kin_dyn;
            agent.kinematic_tree->collect_transforms(kin_static, kin_dyn);
            for (const auto& kft : kin_dyn)
            {
                frame.dynamic_transforms.push_back(
                    {kft.parent_frame, kft.child_frame, kft.relative_pose});
            }
        }

        // Публикуем frame (TF публикуется всегда, сенсоры — только при новых данных)
        adapter_->publish_agent_frame(frame);

        // Опрашиваем события от плагинов
        for (const auto& plugin : agent.plugins)
        {
            auto events = plugin->poll_events();
            for (auto& evt : events)
            {
                evt.agent_id  = agent.id;
                evt.domain_id = agent.domain_id;
                adapter_->emit_event(evt);
            }
        }
    }
}

} // namespace s2
