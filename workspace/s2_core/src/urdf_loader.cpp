/**
 * @file urdf_loader.cpp
 * Реализация загрузчика кинематического дерева из URDF.
 */

#include <s2/urdf_loader.hpp>
#include <tinyxml2.h>

#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>  // std::strtod, std::atof
#include <cstdio>   // std::snprintf

namespace s2
{

namespace
{

// ─── Вспомогательные функции ──────────────────────────────────────────────

// Разобрать строку "x y z" в три double
static bool parse_xyz(const char* str, double& x, double& y, double& z)
{
    if (!str) return false;
    char* end;
    x = std::strtod(str, &end);
    y = std::strtod(end, &end);
    z = std::strtod(end, &end);
    return true;
}

// Разобрать строку "r p y" (roll pitch yaw) в три double
static bool parse_rpy(const char* str, double& r, double& p, double& y)
{
    return parse_xyz(str, r, p, y);
}

// ─── Структура для хранения данных джоинта при парсинге ──────────────────

struct JointDef
{
    std::string type;          ///< "fixed", "revolute", "continuous", "prismatic"
    std::string parent_link;
    std::string child_link;
    Pose3D      origin;        ///< xyz + rpy из <origin>
    Vec3        axis{0,0,1};   ///< из <axis xyz="...">
    double      limit_lower{-M_PI};
    double      limit_upper{ M_PI};
};

} // anonymous namespace

// ─── Реализация ───────────────────────────────────────────────────────────

KinematicTree load_urdf(const std::string& path, const std::string& root_frame)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError err = doc.LoadFile(path.c_str());
    if (err != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error("load_urdf: не удалось открыть '" + path + "': "
                                 + doc.ErrorStr());
    }

    tinyxml2::XMLElement* robot = doc.FirstChildElement("robot");
    if (!robot)
    {
        throw std::runtime_error("load_urdf: элемент <robot> не найден в '" + path + "'");
    }

    // ── Шаг 1: парсим все <joint> ────────────────────────────────────────
    // Карта: child_link → JointDef
    std::unordered_map<std::string, JointDef> joints_by_child;

    for (tinyxml2::XMLElement* jel = robot->FirstChildElement("joint");
         jel != nullptr;
         jel = jel->NextSiblingElement("joint"))
    {
        JointDef def;
        const char* type_attr = jel->Attribute("type");
        def.type = type_attr ? type_attr : "fixed";

        tinyxml2::XMLElement* parent_el = jel->FirstChildElement("parent");
        tinyxml2::XMLElement* child_el  = jel->FirstChildElement("child");
        if (!parent_el || !child_el) continue;

        const char* parent_link_attr = parent_el->Attribute("link");
        const char* child_link_attr  = child_el->Attribute("link");
        if (!parent_link_attr || !child_link_attr) continue;

        def.parent_link = parent_link_attr;
        def.child_link  = child_link_attr;

        // <origin xyz="..." rpy="...">
        tinyxml2::XMLElement* origin_el = jel->FirstChildElement("origin");
        if (origin_el)
        {
            double x = 0, y = 0, z = 0;
            parse_xyz(origin_el->Attribute("xyz"), x, y, z);
            double r = 0, p = 0, yw = 0;
            parse_rpy(origin_el->Attribute("rpy"), r, p, yw);
            def.origin.x     = x;
            def.origin.y     = y;
            def.origin.z     = z;
            def.origin.roll  = r;
            def.origin.pitch = p;
            def.origin.yaw   = yw;
        }

        // <axis xyz="...">
        tinyxml2::XMLElement* axis_el = jel->FirstChildElement("axis");
        if (axis_el)
        {
            double ax = 0, ay = 0, az = 1;
            parse_xyz(axis_el->Attribute("xyz"), ax, ay, az);
            def.axis = Vec3{ax, ay, az};
        }

        // <limit lower="..." upper="...">
        tinyxml2::XMLElement* limit_el = jel->FirstChildElement("limit");
        if (limit_el)
        {
            const char* lower_str = limit_el->Attribute("lower");
            const char* upper_str = limit_el->Attribute("upper");
            if (lower_str) def.limit_lower = std::atof(lower_str);
            if (upper_str) def.limit_upper = std::atof(upper_str);
        }

        joints_by_child[def.child_link] = std::move(def);
    }

    // ── Шаг 1.5: парсим <link> для получения визуальной геометрии ────────
    std::unordered_map<std::string, LinkVisual> link_visuals;

    for (tinyxml2::XMLElement* lel = robot->FirstChildElement("link");
         lel != nullptr;
         lel = lel->NextSiblingElement("link"))
    {
        const char* link_name_attr = lel->Attribute("name");
        if (!link_name_attr) continue;
        std::string link_name = link_name_attr;

        tinyxml2::XMLElement* visual_el = lel->FirstChildElement("visual");
        if (!visual_el) continue;

        tinyxml2::XMLElement* geom_el = visual_el->FirstChildElement("geometry");
        if (!geom_el) continue;

        LinkVisual vis;

        // Визуальный origin (<origin> в <visual>, не в <geometry>)
        tinyxml2::XMLElement* vorigin_el = visual_el->FirstChildElement("origin");
        if (vorigin_el)
        {
            double ox=0, oy=0, oz=0, or_=0, op=0, oy_=0;
            parse_xyz(vorigin_el->Attribute("xyz"), ox, oy, oz);
            parse_rpy(vorigin_el->Attribute("rpy"), or_, op, oy_);
            vis.origin.x = ox; vis.origin.y = oy; vis.origin.z = oz;
            vis.origin.roll = or_; vis.origin.pitch = op; vis.origin.yaw = oy_;
        }

        // Геометрия
        if (tinyxml2::XMLElement* box_el = geom_el->FirstChildElement("box"))
        {
            vis.type = "box";
            double bx=1, by=1, bz=1;
            parse_xyz(box_el->Attribute("size"), bx, by, bz);
            vis.sx = bx; vis.sy = by; vis.sz = bz;
        }
        else if (tinyxml2::XMLElement* cyl_el = geom_el->FirstChildElement("cylinder"))
        {
            vis.type = "cylinder";
            const char* r_attr = cyl_el->Attribute("radius");
            const char* l_attr = cyl_el->Attribute("length");
            if (r_attr) vis.radius = std::atof(r_attr);
            if (l_attr) vis.length = std::atof(l_attr);
        }
        else if (tinyxml2::XMLElement* sph_el = geom_el->FirstChildElement("sphere"))
        {
            vis.type = "sphere";
            const char* r_attr = sph_el->Attribute("radius");
            if (r_attr) vis.radius = std::atof(r_attr);
        }
        else
        {
            // mesh или другое — пропускаем (не поддерживается)
            continue;
        }

        // Цвет из <material><color rgba="...">
        tinyxml2::XMLElement* mat_el = visual_el->FirstChildElement("material");
        if (mat_el)
        {
            tinyxml2::XMLElement* col_el = mat_el->FirstChildElement("color");
            if (col_el)
            {
                const char* rgba = col_el->Attribute("rgba");
                if (rgba)
                {
                    double r=1,g=1,b=1,a=1;
                    char* end;
                    r = std::strtod(rgba, &end);
                    g = std::strtod(end, &end);
                    b = std::strtod(end, &end);
                    // Конвертируем [0..1] в "#RRGGBB"
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                        static_cast<int>(r*255),
                        static_cast<int>(g*255),
                        static_cast<int>(b*255));
                    vis.color = hex;
                }
            }
        }

        link_visuals[link_name] = vis;
    }

    // ── Шаг 2: строим карту parent_link → [child_link, ...] для BFS ──────
    // Инвертируем joints_by_child
    std::unordered_map<std::string, std::vector<std::string>> children_of;
    for (const auto& [child, jdef] : joints_by_child)
    {
        children_of[jdef.parent_link].push_back(child);
    }

    // ── Шаг 3: BFS от root_frame ─────────────────────────────────────────
    KinematicTree tree;

    // Корневое звено (base_link или другой root_frame) — без джоинта
    {
        Link root_link;
        root_link.name   = root_frame;
        root_link.parent = "";
        root_link.joint.type = JointType::FIXED;
        auto vit = link_visuals.find(root_frame);
        if (vit != link_visuals.end()) root_link.visual = vit->second;
        tree.add_link(std::move(root_link));
    }

    std::queue<std::string> bfs_queue;
    bfs_queue.push(root_frame);

    while (!bfs_queue.empty())
    {
        std::string current = bfs_queue.front();
        bfs_queue.pop();

        auto it = children_of.find(current);
        if (it == children_of.end()) continue;

        for (const std::string& child_name : it->second)
        {
            const JointDef& jdef = joints_by_child.at(child_name);

            Link lk;
            lk.name   = child_name;
            lk.parent = current;
            lk.origin = jdef.origin;

            // Маппинг типа джоинта
            if      (jdef.type == "revolute")   lk.joint.type = JointType::REVOLUTE;
            else if (jdef.type == "continuous")  lk.joint.type = JointType::CONTINUOUS;
            else if (jdef.type == "prismatic")   lk.joint.type = JointType::PRISMATIC;
            else                                  lk.joint.type = JointType::FIXED;

            lk.joint.axis  = jdef.axis;
            lk.joint.min   = jdef.limit_lower;
            lk.joint.max   = jdef.limit_upper;
            lk.joint.value = 0.0;

            auto vit = link_visuals.find(child_name);
            if (vit != link_visuals.end()) lk.visual = vit->second;

            tree.add_link(std::move(lk));
            bfs_queue.push(child_name);
        }
    }

    return tree;
}

} // namespace s2
