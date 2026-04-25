#include <gtest/gtest.h>
#include <s2/zone_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/types.hpp>

namespace s2 {

// ─── Вспомогательные функции ──────────────────────────────────────────────────

static Agent make_agent(AgentId id, double x, double y, double z = 0.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = z;
    return a;
}

static Zone make_sphere_zone(const ZoneId& id, Vec3 center, double radius)
{
    Zone z;
    z.id              = id;
    z.enabled         = true;
    z.shape.type      = ZoneShapeType::SPHERE;
    z.shape.center    = center;
    z.shape.radius    = radius;
    return z;
}

static Zone make_aabb_zone(const ZoneId& id, Vec3 center, Vec3 half_size)
{
    Zone z;
    z.id              = id;
    z.enabled         = true;
    z.shape.type      = ZoneShapeType::AABB;
    z.shape.center    = center;
    z.shape.half_size = half_size;
    return z;
}

static Zone make_cylinder_zone(const ZoneId& id, Vec3 center, double radius, double half_height)
{
    Zone z;
    z.id                  = id;
    z.enabled             = true;
    z.shape.type          = ZoneShapeType::CYLINDER;
    z.shape.center        = center;
    z.shape.radius        = radius;
    z.shape.half_height   = half_height;
    return z;
}

// ─── Тест 1: ZoneShape::contains() для CYLINDER ──────────────────────────────

TEST(ZoneShape_CylinderContains, VariousPoints)
{
    ZoneShape s;
    s.type        = ZoneShapeType::CYLINDER;
    s.center      = Vec3{0.0, 0.0, 1.0};
    s.radius      = 2.0;
    s.half_height = 0.5;

    // Внутри
    EXPECT_TRUE(s.contains(Vec3{0.0, 0.0, 1.0}));    // центр
    EXPECT_TRUE(s.contains(Vec3{1.9, 0.0, 1.0}));    // у края радиуса
    EXPECT_TRUE(s.contains(Vec3{0.0, 0.0, 1.5}));    // у верхнего края
    EXPECT_TRUE(s.contains(Vec3{0.0, 0.0, 0.5}));    // у нижнего края
    EXPECT_TRUE(s.contains(Vec3{0.0, 0.0, 1.0}));    // граница включена

    // Снаружи
    EXPECT_FALSE(s.contains(Vec3{2.1, 0.0, 1.0}));   // за радиусом
    EXPECT_FALSE(s.contains(Vec3{0.0, 0.0, 2.0}));   // выше цилиндра
    EXPECT_FALSE(s.contains(Vec3{0.0, 0.0, 0.0}));   // ниже цилиндра
    EXPECT_FALSE(s.contains(Vec3{1.5, 1.5, 1.0}));   // по диагонали за радиусом
}

// ─── Тест 2: Агент входит в сферическую зону → AgentEnteredZone event ─────────

TEST(ZoneSystem_AgentEnterSphere, EventPublished)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 2.0));

    SimBus bus;
    AgentId entered_agent = 999;
    ZoneId  entered_zone;
    bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone& e) {
        entered_agent = e.agent;
        entered_zone  = e.zone;
    });

    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 1.0, 0.0, 0.0));
    std::vector<Actor> actors;

    zs.tick(agents, actors, bus, 0.0, 0.01);

    EXPECT_EQ(entered_agent, 1u);
    EXPECT_EQ(entered_zone, "zone1");
    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);
}

// ─── Тест 3: Агент выходит из зоны → AgentExitedZone event ───────────────────

TEST(ZoneSystem_AgentExitSphere, EventPublished)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 2.0));

    SimBus bus;

    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 1.0, 0.0, 0.0));
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);  // агент внутри

    AgentId exited_agent = 999;
    ZoneId  exited_zone;
    bus.subscribe<event::AgentExitedZone>([&](const event::AgentExitedZone& e) {
        exited_agent = e.agent;
        exited_zone  = e.zone;
    });

    // Вывести агента за пределы зоны
    agents[0].world_pose.x = 5.0;
    zs.tick(agents, actors, bus, 0.01, 0.01);

    EXPECT_EQ(exited_agent, 1u);
    EXPECT_EQ(exited_zone, "zone1");
    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 0u);
}

// ─── Тест 4: Агент снаружи → никаких событий ─────────────────────────────────

TEST(ZoneSystem_NoEventIfOutside, NoEvents)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 1.0));

    SimBus bus;
    int event_count = 0;
    bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone&) { ++event_count; });
    bus.subscribe<event::AgentExitedZone>([&](const event::AgentExitedZone&) { ++event_count; });

    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 5.0, 0.0, 0.0));  // далеко
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);
    zs.tick(agents, actors, bus, 0.01, 0.01);

    EXPECT_EQ(event_count, 0);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.size(), 0u);
}

// ─── Тест 5: Две AABB на разных Z → агент на z=0 только в нижней ─────────────

TEST(ZoneSystem_AABB_MultiFloor, OnlyInLowerZone)
{
    ZoneSystem zs;
    // Нижняя зона: z от -1 до +1
    zs.add_zone(make_aabb_zone("lower", Vec3{0.0, 0.0, 0.0}, Vec3{5.0, 5.0, 1.0}));
    // Верхняя зона: z от +2 до +4
    zs.add_zone(make_aabb_zone("upper", Vec3{0.0, 0.0, 3.0}, Vec3{5.0, 5.0, 1.0}));

    SimBus bus;
    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);  // lower
    EXPECT_EQ(zs.all_zones()[1].inside_agents.count(1u), 0u);  // upper
}

// ─── Тест 6: Агент в CYLINDER → inside; выше/ниже → нет ─────────────────────

TEST(ZoneSystem_CylinderContains, AboveBelowOutside)
{
    // Внутри (на уровне z=1)
    ZoneSystem zs;
    zs.add_zone(make_cylinder_zone("cyl", Vec3{0.0, 0.0, 1.0}, 2.0, 0.5));

    SimBus bus;
    std::vector<Actor> actors;

    std::vector<Agent> agents_inside;
    agents_inside.push_back(make_agent(1, 1.0, 0.0, 1.0));
    zs.tick(agents_inside, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);

    // Выше цилиндра (z=2)
    ZoneSystem zs2;
    zs2.add_zone(make_cylinder_zone("cyl", Vec3{0.0, 0.0, 1.0}, 2.0, 0.5));
    std::vector<Agent> agents_above;
    agents_above.push_back(make_agent(1, 1.0, 0.0, 2.0));
    zs2.tick(agents_above, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs2.all_zones()[0].inside_agents.count(1u), 0u);

    // Ниже цилиндра (z=0)
    ZoneSystem zs3;
    zs3.add_zone(make_cylinder_zone("cyl", Vec3{0.0, 0.0, 1.0}, 2.0, 0.5));
    std::vector<Agent> agents_below;
    agents_below.push_back(make_agent(1, 1.0, 0.0, 0.0));
    zs3.tick(agents_below, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs3.all_zones()[0].inside_agents.count(1u), 0u);
}

// ─── Тест 7: Зона attached к агенту → следует за ним ─────────────────────────

TEST(ZoneSystem_AttachedZone, FollowsAgent)
{
    ZoneSystem zs;

    Zone zone = make_sphere_zone("attached", Vec3{0.0, 0.0, 0.0}, 1.0);
    zone.attached_to_agent  = AgentId{0};
    zone.attachment_offset  = Vec3{0.0, 0.0, 0.0};
    zs.add_zone(std::move(zone));

    SimBus bus;
    std::vector<Actor> actors;

    // Агент двигается к позиции (10, 0, 0)
    std::vector<Agent> agents;
    agents.push_back(make_agent(0, 10.0, 0.0, 0.0));
    zs.tick(agents, actors, bus, 0.0, 0.01);

    // Зона должна сместиться к агенту
    EXPECT_DOUBLE_EQ(zs.all_zones()[0].shape.center.x(), 10.0);
    EXPECT_DOUBLE_EQ(zs.all_zones()[0].shape.center.y(), 0.0);
}

// ─── Тест 8: Зона disabled → агент внутри не получает enter event ────────────

TEST(ZoneSystem_DisabledZone, NoEnterEvent)
{
    ZoneSystem zs;
    Zone zone = make_sphere_zone("disabled_zone", Vec3{0.0, 0.0, 0.0}, 5.0);
    zone.enabled = false;
    zs.add_zone(std::move(zone));

    SimBus bus;
    int enter_count = 0;
    bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone&) { ++enter_count; });

    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    EXPECT_EQ(enter_count, 0);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.size(), 0u);
}

// ─── Тест 9: zones_containing() возвращает правильный список ─────────────────

TEST(ZoneSystem_ZonesContaining, CorrectList)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("z1", Vec3{0.0, 0.0, 0.0}, 2.0));
    zs.add_zone(make_sphere_zone("z2", Vec3{10.0, 0.0, 0.0}, 2.0));
    zs.add_zone(make_aabb_zone("z3", Vec3{0.0, 0.0, 0.0}, Vec3{5.0, 5.0, 5.0}));

    // Точка (0,0,0) — в z1 и z3
    auto result = zs.zones_containing(Vec3{0.0, 0.0, 0.0});
    ASSERT_EQ(result.size(), 2u);
    EXPECT_TRUE(std::find(result.begin(), result.end(), "z1") != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), "z3") != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), "z2") == result.end());

    // Точка (10,0,0) — только в z2
    auto result2 = zs.zones_containing(Vec3{10.0, 0.0, 0.0});
    ASSERT_EQ(result2.size(), 1u);
    EXPECT_EQ(result2[0], "z2");
}

// ─── Тест 10: resize_zone() → новые размеры в следующем тике ─────────────────

TEST(ZoneSystem_RuntimeResize, ResizeApplied)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 1.0));

    SimBus bus;
    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 1.5, 0.0, 0.0));
    std::vector<Actor> actors;

    // Агент на расстоянии 1.5 от центра — снаружи (радиус 1.0)
    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.size(), 0u);

    // Увеличиваем радиус
    ZoneShape new_shape;
    new_shape.type   = ZoneShapeType::SPHERE;
    new_shape.center = Vec3{0.0, 0.0, 0.0};
    new_shape.radius = 3.0;
    EXPECT_TRUE(zs.resize_zone("zone1", new_shape));

    // Следующий тик — агент теперь внутри
    int enter_count = 0;
    bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone&) { ++enter_count; });
    zs.tick(agents, actors, bus, 0.01, 0.01);

    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);
    EXPECT_EQ(enter_count, 1);
}

// ─── Тест 11: capabilities — агент без нужного capability → MODIFIER не вызывается

// Stub-плагин для проверки вызовов
class StubModifierPlugin : public EffectPlugin
{
public:
    int apply_count = 0;

    void on_init(const YAML::Node&) override {}
    EffectType effect_type() const override { return EffectType::MODIFIER; }
    std::vector<std::string> required_capabilities() const override
    {
        return {"special_capability"};
    }
    void apply_modifier(SharedState&, const EffectContext&) override
    {
        ++apply_count;
    }
};

TEST(ZoneSystem_CapabilitiesMatch, NoCapabilityNoEffect)
{
    ZoneSystem zs;

    Zone zone = make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 5.0);

    Zone::EffectDesc desc;
    desc.type             = "stub";
    desc.enabled          = true;
    desc.effect_type      = EffectType::MODIFIER;
    desc.required_capabilities = {"special_capability"};

    auto stub = std::make_unique<StubModifierPlugin>();
    StubModifierPlugin* raw_stub = stub.get();
    desc.plugin = std::move(stub);

    zone.effects.push_back(std::move(desc));
    zs.add_zone(std::move(zone));

    SimBus bus;
    std::vector<Actor> actors;

    // Агент БЕЗ нужного capability
    std::vector<Agent> agents_no;
    agents_no.push_back(make_agent(1, 0.0, 0.0, 0.0));
    zs.tick(agents_no, actors, bus, 0.0, 0.01);
    EXPECT_EQ(raw_stub->apply_count, 0);

    // Агент С нужным capability
    ZoneSystem zs2;
    Zone zone2 = make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 5.0);

    Zone::EffectDesc desc2;
    desc2.type             = "stub";
    desc2.enabled          = true;
    desc2.effect_type      = EffectType::MODIFIER;
    desc2.required_capabilities = {"special_capability"};

    auto stub2 = std::make_unique<StubModifierPlugin>();
    StubModifierPlugin* raw_stub2 = stub2.get();
    desc2.plugin = std::move(stub2);

    zone2.effects.push_back(std::move(desc2));
    zs2.add_zone(std::move(zone2));

    Agent agent_with_cap = make_agent(2, 0.0, 0.0, 0.0);
    agent_with_cap.capabilities.insert("special_capability");
    std::vector<Agent> agents_cap;
    agents_cap.push_back(std::move(agent_with_cap));
    zs2.tick(agents_cap, actors, bus, 0.0, 0.01);
    EXPECT_EQ(raw_stub2->apply_count, 1);
}

// ─── Тест N: ZoneSystem публикует event::ZoneEntered при входе агента ────────

TEST(ZoneSystem_ZoneEnteredEvent, PublishedOnAgentEnter)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("charging_zone", Vec3{0.0, 0.0, 0.0}, 2.0));

    SimBus bus;
    ZoneId   received_zone;
    EntityId received_entity = 0;

    bus.subscribe<event::ZoneEntered>([&](const event::ZoneEntered& e) {
        received_zone   = e.zone_id;
        received_entity = e.entity_id;
    });

    std::vector<Agent> agents;
    agents.push_back(make_agent(7, 1.0, 0.0, 0.0));  // внутри зоны
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    EXPECT_EQ(received_zone,   "charging_zone");
    EXPECT_EQ(received_entity, 7u);
}

// ─── Тест N+1: ZoneSystem публикует event::ZoneExited при выходе агента ──────

TEST(ZoneSystem_ZoneExitedEvent, PublishedOnAgentExit)
{
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("charging_zone", Vec3{0.0, 0.0, 0.0}, 2.0));

    SimBus bus;

    // Сначала ввести агента в зону
    std::vector<Agent> agents;
    agents.push_back(make_agent(7, 1.0, 0.0, 0.0));
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    ZoneId   received_zone;
    EntityId received_entity = 0;

    bus.subscribe<event::ZoneExited>([&](const event::ZoneExited& e) {
        received_zone   = e.zone_id;
        received_entity = e.entity_id;
    });

    // Вывести агента за пределы зоны
    agents[0].world_pose.x = 5.0;
    zs.tick(agents, actors, bus, 0.01, 0.01);

    EXPECT_EQ(received_zone,   "charging_zone");
    EXPECT_EQ(received_entity, 7u);
}

} // namespace s2
