import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';

// ============================================================
// Глобальное состояние
// ============================================================
let followMode = false;
let selectedAgentId = null;
let selectedAgentMesh = null;
let lastAgentData = {};   // ID -> полный JSON агент
let pluginsData = {};     // plugins_data из сервера
let isPaused = false;
let defaultCameraPosition = new THREE.Vector3(12, 12, 12);
let defaultCameraTarget = new THREE.Vector3(0, 0, 0);
let transformMode = 'translate'; // 'translate' или 'rotate'
let previousUpdate = {}; // для отслеживания изменений позиции
let isDragging = false;  // флажок: идёт ли перетаскивание агента
let dragReleaseTime = 0; // время окончания перетаскивания
let pluginAccordionState = {}; // pluginName -> open (true/false)
let pluginInputsSchemas = {};  // agentKey -> { plugin_type -> schema }
let pluginInputIntervals = {}; // "${agentId}-${pluginName}" -> intervalId
let pluginInputLastValues = {}; // "${agentId}-${pluginName}" -> {fieldName: value}
let tfEnabledAgents = new Set(); // Set of agent IDs that have TF frames enabled
let lastSimTime = null;          // для обнаружения reset симуляции
const tfFrames = {};             // agentKey -> AxesHelper
const linkMeshes = {};           // `lm_${agentId}_${linkName}` -> Mesh (per-URDF-link render)
const agentHasUrdf = {};         // agentId -> bool

// ============================================================
// Инициализация сцены
// ============================================================
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x1a1a2e);

const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 500);
camera.position.copy(defaultCameraPosition);
camera.lookAt(0, 0, 0);

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
document.body.appendChild(renderer.domElement);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.dampingFactor = 0.08;
controls.target.copy(defaultCameraTarget);

// TransformControls для перетаскивания агентов
const transformControls = new TransformControls(camera, renderer.domElement);
transformControls.addEventListener('dragging-changed', function (event) {
    controls.enabled = !event.value;
    isDragging = event.value;
    if (!event.value) {
        dragReleaseTime = Date.now();
    }
});

// Задержка после отпускания гизмо: не перезаписываем позицию из SSE пока сервер не обработал move
const DRAG_GRACE_MS = 800;

transformControls.addEventListener('mouseUp', function () {
    if (selectedAgentId !== null && selectedAgentMesh) {
        const m = selectedAgentMesh;
        const x = m.position.x;
        const y = -m.position.z;
        const yaw = m.rotation.y;
        sendMoveAgent(selectedAgentId, x, y, yaw);
    }
});
scene.add(transformControls);

// ============================================================
// Освещение
// ============================================================
const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
scene.add(ambientLight);

const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
directionalLight.position.set(10, 20, 10);
directionalLight.castShadow = true;
directionalLight.shadow.mapSize.width = 2048;
directionalLight.shadow.mapSize.height = 2048;
scene.add(directionalLight);

// ============================================================
// Сетка
// ============================================================
const gridHelper = new THREE.GridHelper(40, 40, 0x444466, 0x222244);
scene.add(gridHelper);

// ============================================================
// Хранилище мешей
// ============================================================
const meshes = {};

// ============================================================
// Overlay lines (trajectory, path)
// ============================================================
const overlayLines = {};      // id -> THREE.Line
const overlayLineCache = {};  // id -> { pointCount, lastPoint0, lastPoint1 } — для детектирования изменений

function renderOverlayLine(id, points, color) {
    if (!points || points.length < 2) {
        clearOverlayLine(id);
        return;
    }

    // Быстрая проверка изменений: сравниваем количество точек и первую/последнюю
    const cache = overlayLineCache[id];
    const last = points[points.length - 1];
    if (cache &&
        cache.pointCount === points.length &&
        cache.lastX === last[0] && cache.lastY === last[1]) {
        // Точки не изменились — обновлять геометрию не нужно
        return;
    }

    // Удалить старую линию
    if (overlayLines[id]) {
        overlayLines[id].geometry.dispose();
        overlayLines[id].material.dispose();
        scene.remove(overlayLines[id]);
        delete overlayLines[id];
    }

    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(points.flatMap(p => [p[0], p[2], -p[1]])); // Y-up
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const material = new THREE.LineBasicMaterial({ color: color, linewidth: 2 });
    const line = new THREE.Line(geometry, material);
    scene.add(line);
    overlayLines[id] = line;

    overlayLineCache[id] = { pointCount: points.length, lastX: last[0], lastY: last[1] };
}

function clearOverlayLine(id) {
    if (overlayLines[id]) {
        overlayLines[id].geometry.dispose();
        overlayLines[id].material.dispose();
        scene.remove(overlayLines[id]);
        delete overlayLines[id];
    }
    delete overlayLineCache[id];
}

function clearOverlayLines() {
    for (const id of Object.keys(overlayLines)) {
        clearOverlayLine(id);
    }
}

// ============================================================
// Вспомогательные функции
// ============================================================
function hexToColor(hex) {
    if (!hex) return 0xff6b35;
    if (hex.startsWith('#')) hex = hex.slice(1);
    return parseInt(hex, 16);
}

function createGeometry(type, size, radius, height) {
    switch (type) {
        case 'sphere':
            return new THREE.SphereGeometry(radius || size.x / 2 || 0.5, 16, 16);
        case 'cylinder':
            return new THREE.CylinderGeometry(radius || 0.5, radius || 0.5, height || 1, 16);
        case 'capsule':
            return new THREE.CapsuleGeometry(radius || 0.5, height || 1, 4, 8);
        case 'box':
        default:
            return new THREE.BoxGeometry(
                size.x !== undefined ? size.x : 1,
                size.y !== undefined ? size.y : 1,
                size.z !== undefined ? size.z : 1
            );
    }
}

function updateOrCreateMesh(key, type, pose, visual, opts = {}) {
    let mesh = meshes[key];
    if (!mesh) {
        const size = { x: 1, y: 1, z: 1 };
        if (visual?.size) {
            size.x = visual.size[0] || 1;
            size.y = visual.size[1] || 1;
            size.z = visual.size[2] || 1;
        }
        const radius = visual?.radius || 0.5;
        const height = visual?.height || 1.0;

        const geomType = opts.forceType || (visual?.type || 'box');
        const geometry = createGeometry(geomType, size, radius, height);
        const material = new THREE.MeshStandardMaterial({
            color: hexToColor(visual?.color),
            transparent: opts.wireframe || false,
            opacity: opts.opacity || 1.0,
            wireframe: opts.wireframe || false,
        });

        mesh = new THREE.Mesh(geometry, material);
        mesh.castShadow = !opts.wireframe;
        mesh.receiveShadow = true;
        mesh.userData.key = key;
        scene.add(mesh);
        meshes[key] = mesh;
    } else {
        const newColor = hexToColor(visual?.color);
        if (mesh.material.color.getHex() !== newColor) {
            mesh.material.color.setHex(newColor);
        }
    }

    mesh.position.set(pose.x || 0, pose.z || 0, -(pose.y || 0));
    mesh.rotation.y = pose.yaw || 0;

    return mesh;
}

function removeMesh(key) {
    const mesh = meshes[key];
    if (mesh) {
        scene.remove(mesh);
        mesh.geometry?.dispose();
        if (Array.isArray(mesh.material)) {
            mesh.material.forEach(m => m.dispose());
        } else {
            mesh.material?.dispose();
        }
        delete meshes[key];
    }
}

// Текущий агент, для которого построен аккордеон
let currentAccordionAgentId = null;

// ============================================================
// Аккордеон плагинов — полностью пересоздаётся только при смене агента.
// При каждом обновлении — только обновляется JSON-контент.
// ============================================================
function updatePluginAccordion(agentId) {
    const container = document.getElementById('plugin-container');
    const agentKey = `agent_${agentId}`;
    const agentPlugins = pluginsData[agentKey] || {};
    const pluginKeys = Object.keys(agentPlugins);

    // Если агент сменился — пересоздаём весь контейнер
    if (agentId !== currentAccordionAgentId) {
        pluginAccordionState = {};
        container.innerHTML = '';
        currentAccordionAgentId = agentId;

        if (pluginKeys.length === 0) {
            container.innerHTML = '<div style="color: #666; font-size: 12px;">нет данных</div>';
            return;
        }

        for (const pluginName of pluginKeys) {
            const pluginData = agentPlugins[pluginName];
            const jsonData = typeof pluginData === 'string' ? pluginData : JSON.stringify(pluginData, null, 2);

            const accordion = document.createElement('div');
            accordion.className = 'plugin-accordion';
            accordion.dataset.agentId = agentId;
            const hasInput = pluginInputsSchemas[agentKey] && pluginInputsSchemas[agentKey][pluginName];
            const inputBtnHtml = hasInput ? `<button class="plugin-input-btn" data-plugin="${pluginName}" onclick="showPluginInputForm('${agentKey}', '${pluginName}')">⚙️</button>` : '';
            accordion.innerHTML = `
                <div class="plugin-header" data-plugin="${pluginName}">
                    <span>${pluginName}</span>
                    <span style="display:flex;align-items:center;gap:6px;">${inputBtnHtml}<span class="arrow">▶</span></span>
                </div>
                <div class="plugin-content" id="plugin-content-${agentId}-${pluginName}">${escapeHtml(jsonData)}</div>
                <div class="plugin-input-form" id="plugin-form-${agentId}-${pluginName}" style="display:none;"></div>
            `;
            container.appendChild(accordion);
        }
        return;
    }

    // Агент тот же — обновляем только JSON-контент в открытых секциях
    for (const pluginName of pluginKeys) {
        const pluginData = agentPlugins[pluginName];
        const jsonData = typeof pluginData === 'string' ? pluginData : JSON.stringify(pluginData, null, 2);
        const contentEl = document.getElementById(`plugin-content-${agentId}-${pluginName}`);
        if (contentEl && contentEl.textContent !== jsonData) {
            contentEl.textContent = jsonData;
        }
    }
}

// Event delegation для кликов по аккордеону
document.getElementById('plugin-container').addEventListener('click', function(e) {
    if (e.target.closest('.plugin-input-btn')) return;
    const header = e.target.closest('.plugin-header');
    if (!header) return;

    const pluginName = header.dataset.plugin;
    const accordion = header.closest('.plugin-accordion');
    if (!accordion) return;

    const content = accordion.querySelector('.plugin-content');
    const arrow = header.querySelector('.arrow');
    const isOpen = content.classList.contains('open');

    header.classList.toggle('open', !isOpen);
    content.classList.toggle('open', !isOpen);
    arrow.textContent = !isOpen ? '▼' : '▶';
    pluginAccordionState[pluginName] = !isOpen;
});

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ============================================================
// Обновление боковой панели
// ============================================================
function updateSidePanel(agent) {
    if (!agent) {
        closeSidePanel();
        return;
    }

    document.getElementById('sp-agent-id').textContent = agent.id;
    document.getElementById('sp-agent-name').textContent = agent.name;
    document.getElementById('sp-pos-x').textContent = (agent.pose?.x || 0).toFixed(3);
    document.getElementById('sp-pos-y').textContent = (agent.pose?.y || 0).toFixed(3);
    document.getElementById('sp-pos-z').textContent = (agent.pose?.z || 0).toFixed(3);
    document.getElementById('sp-yaw').textContent = (agent.pose?.yaw || 0).toFixed(3);
    document.getElementById('sp-vx').textContent = (agent.velocity?.vx || 0).toFixed(3);
    document.getElementById('sp-vy').textContent = (agent.velocity?.vy || 0).toFixed(3);
    document.getElementById('sp-wz').textContent = (agent.velocity?.wz || 0).toFixed(3);

    // Сбрасываем состояние при смене агента
    pluginAccordionState = {};
    updatePluginAccordion(agent.id);

    document.getElementById('side-panel').classList.add('visible');
}

function closeSidePanel() {
    document.getElementById('side-panel').classList.remove('visible');
    if (transformControls.object) {
        transformControls.detach();
    }
    // Удалить TF frames для предыдущего агента
    if (selectedAgentId !== null) {
        const key = `agent_${selectedAgentId}`;
        if (tfFrames[key]) {
            scene.remove(tfFrames[key]);
            delete tfFrames[key];
        }
    }
    selectedAgentId = null;
    selectedAgentMesh = null;
    pluginAccordionState = {};
}

// ============================================================
// TF Frames (AxesHelper)
// ============================================================
// AxesHelper: X=red (forward), Y=green (left), Z=blue (up)
// Для Three.js робот смотрит по Z-оси (или X?), нужно настроить вращение
// Mesh robota: rotation.y = -yaw (Three.js rotation вокруг Y)
// TF: красная = X робота (вперёд), зелёная = Y (влево), синяя = Z (вверх)

function getOrCreateTFFrame(agentKey) {
    if (!tfFrames[agentKey]) {
        const axes = new THREE.AxesHelper(2.0);
        axes.visible = false;
        scene.add(axes);
        tfFrames[agentKey] = axes;
    }
    return tfFrames[agentKey];
}

function updateTFFrameForAgent(agentKey) {
    const mesh = meshes[agentKey];
    if (!mesh) return;
    const axes = getOrCreateTFFrame(agentKey);
    axes.position.copy(mesh.position);
    // Важно: используем то же вращение что и у меша
    axes.rotation.copy(mesh.rotation);
    // Показать если агент в enabled set
    const agentId = parseInt(agentKey.replace('agent_', ''));
    axes.visible = tfEnabledAgents.has(agentId);
}

// Обновить все видимые TF frames
function updateAllTFFrames() {
    for (const agentId of tfEnabledAgents) {
        const key = `agent_${agentId}`;
        if (meshes[key]) {
            updateTFFrameForAgent(key);
        }
    }
}

/**
 * Создать или обновить AxesHelper для звена кинематического дерева.
 * @param frameKey   Уникальный ключ (например "tf_0_arm")
 * @param agentId    ID агента (для проверки видимости)
 * @param pose       {x, y, z, yaw} в мировых координатах
 */
function updateOrCreateKinematicFrame(frameKey, agentId, pose) {
    if (!tfFrames[frameKey]) {
        const axes = new THREE.AxesHelper(1.0);
        axes.visible = false;
        scene.add(axes);
        tfFrames[frameKey] = axes;
    }
    const axes = tfFrames[frameKey];
    // Координаты: Three.js использует Y-up, симулятор — Z-up
    axes.position.set(pose.x || 0, pose.z || 0, -(pose.y || 0));
    // Z-up (sim) → Y-up (Three.js): Rz(yaw)*Ry(pitch)*Rx(roll) → Ry(yaw)*Rz(-pitch)*Rx(roll)
    // Euler order 'YZX', pitch инвертируется из-за смены handedness оси Y→-Z
    axes.rotation.set(pose.roll || 0, pose.yaw || 0, -(pose.pitch || 0), 'YZX');
    axes.visible = tfEnabledAgents.has(agentId);
}

/**
 * Создать или обновить меш для звена кинематического дерева (URDF visual).
 * @param lmKey   `lm_${agentId}_${linkName}`
 * @param pose    {x, y, z, yaw} мировая поза звена (Z-up → Y-up)
 * @param visual  {type, color, sx, sy, sz, radius, length}
 */
function updateOrCreateLinkMesh(lmKey, pose, visual) {
    let mesh = linkMeshes[lmKey];
    if (!mesh) {
        let geometry;
        if (visual.type === 'cylinder') {
            // URDF цилиндр: ось вдоль Z, Three.js CylinderGeometry — вдоль Y
            // Поворачиваем геометрию на 90° вокруг X чтобы ось совпала
            geometry = new THREE.CylinderGeometry(visual.radius, visual.radius, visual.length, 16);
            geometry.applyMatrix4(new THREE.Matrix4().makeRotationX(Math.PI / 2));
        } else if (visual.type === 'sphere') {
            geometry = new THREE.SphereGeometry(visual.radius, 16, 16);
        } else {
            geometry = new THREE.BoxGeometry(visual.sx || 1, visual.sz || 1, visual.sy || 1);
        }
        const material = new THREE.MeshStandardMaterial({ color: visual.color || '#888888' });
        mesh = new THREE.Mesh(geometry, material);
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        mesh.userData.lmKey = lmKey;
        mesh.userData.agentId = parseInt(lmKey.split('_')[1]);
        scene.add(mesh);
        linkMeshes[lmKey] = mesh;
    } else {
        const newColor = parseInt((visual.color || '#888888').replace('#', '0x'));
        if (mesh.material.color.getHex() !== newColor) {
            mesh.material.color.setHex(newColor);
        }
    }
    // Координатное преобразование Z-up (sim) → Y-up (Three.js)
    mesh.position.set(pose.x || 0, pose.z || 0, -(pose.y || 0));
    // Ориентация: применяем yaw + pitch + roll из Pose3D
    // Z-up (sim) → Y-up (Three.js): Rz(yaw)*Ry(pitch)*Rx(roll) → Ry(yaw)*Rz(-pitch)*Rx(roll)
    mesh.rotation.set(pose.roll || 0, pose.yaw || 0, -(pose.pitch || 0), 'YZX');
    return mesh;
}

function removeLinkMesh(key) {
    const mesh = linkMeshes[key];
    if (mesh) {
        scene.remove(mesh);
        mesh.geometry?.dispose();
        mesh.material?.dispose();
        delete linkMeshes[key];
    }
}

// Checkbox — включаем TF только для выбранного агента
document.getElementById('tf-frames-toggle').addEventListener('change', function(e) {
    if (selectedAgentId !== null) {
        if (e.target.checked) {
            tfEnabledAgents.add(selectedAgentId);
        } else {
            tfEnabledAgents.delete(selectedAgentId);
        }
        // Обновить видимость TF frames: base_link агентов
        for (const [key, axes] of Object.entries(tfFrames)) {
            let id;
            if (key.startsWith('agent_')) {
                id = parseInt(key.replace('agent_', ''));
            } else if (key.startsWith('tf_')) {
                // tf_<agentId>_<frame_name>
                id = parseInt(key.split('_')[1]);
            } else {
                continue;
            }
            axes.visible = tfEnabledAgents.has(id);
        }
    }
});

// ============================================================
// Обновление сцены из JSON
// ============================================================
let geometrySent = false;

function updateScene(data) {
    // SimTime — обнаруживаем reset (время идёт назад) и очищаем overlay-линии
    if (data.sim_time !== undefined) {
        document.getElementById('sim-time').textContent = data.sim_time.toFixed(2) + 's';
        if (lastSimTime !== null && data.sim_time < lastSimTime - 0.5) {
            clearOverlayLines();
        }
        lastSimTime = data.sim_time;
    }

    // Paused — обновляем UI
    if (data.paused !== undefined) {
        isPaused = data.paused;
        const indicator = document.getElementById('pause-indicator');
        indicator.style.display = isPaused ? 'block' : 'none';
        const btn = document.getElementById('btn-pause');
        if (btn) {
            btn.textContent = isPaused ? 'Play' : 'Pause';
        }
    }

    // Сохраняем plugins_data
    if (data.plugins_data) {
        pluginsData = data.plugins_data;
        // Обновляем аккордеон если панель открыта
        if (selectedAgentId !== null) {
            updatePluginAccordion(selectedAgentId);
        }

        // Рендерим overlay-линии для каждого агента
        for (const [agentKey, agentPlugins] of Object.entries(data.plugins_data)) {
            // agentKey: "agent_0", "agent_1", ...
            try {
                if (agentPlugins.trajectory_recorder) {
                    const d = typeof agentPlugins.trajectory_recorder === 'string'
                        ? JSON.parse(agentPlugins.trajectory_recorder)
                        : agentPlugins.trajectory_recorder;
                    if (d.enabled === false) {
                        clearOverlayLine(`traj_${agentKey}`);
                    } else {
                        renderOverlayLine(`traj_${agentKey}`, d.points, d.color);
                    }
                }
                if (agentPlugins.path_display) {
                    const d = typeof agentPlugins.path_display === 'string'
                        ? JSON.parse(agentPlugins.path_display)
                        : agentPlugins.path_display;
                    if (d.visible === false) {
                        clearOverlayLine(`path_${agentKey}`);
                    } else {
                        renderOverlayLine(`path_${agentKey}`, d.points, d.color);
                    }
                }
            } catch (e) {
                console.error('[overlay] parse error for', agentKey, e);
            }
        }
    }

    // Сохраняем схемы входных данных плагинов (приходят в каждом снапшоте, но не меняются)
    // В снапшоте значения могут быть JSON-строками ИЛИ уже объектами
    if (data.plugin_inputs_schemas) {
        let needRebuild = false;
        for (const [agentKey, plugins] of Object.entries(data.plugin_inputs_schemas)) {
            if (!pluginInputsSchemas[agentKey]) {
                pluginInputsSchemas[agentKey] = {};
            }
            
            for (const [pluginName, schema] of Object.entries(plugins)) {
                if (pluginInputsSchemas[agentKey][pluginName]) continue; // уже загружено
                
                needRebuild = true;
                try {
                    if (typeof schema === 'string') {
                        pluginInputsSchemas[agentKey][pluginName] = JSON.parse(schema);
                    } else {
                        pluginInputsSchemas[agentKey][pluginName] = schema;
                    }
                } catch (e) {
                    console.error(`Failed to parse plugin_inputs_schemas for ${agentKey}/${pluginName}:`, e);
                    pluginInputsSchemas[agentKey][pluginName] = {};
                }
            }
        }
        // Пересоздаём аккордеон только при первом получении схем (чтобы появились кнопки ⚙️)
        if (needRebuild && selectedAgentId !== null) {
            currentAccordionAgentId = null;
            updatePluginAccordion(selectedAgentId);
        }
    }

    // Геометрия (только при первом подключении)
    if (data.geometry && !geometrySent) {
        geometrySent = true;
        Object.keys(meshes).forEach(k => { if (k.startsWith('static_')) removeMesh(k); });

        data.geometry.forEach((geom, i) => {
            const pose = { x: geom.x || 0, y: geom.y || 0, z: geom.z || 0, yaw: 0 };
            const visual = {
                type: geom.type || 'box',
                size: [geom.sx || 1, geom.sy || 1, geom.sz || 1],
                color: geom.color || '#808080',
            };
            updateOrCreateMesh(`static_${i}`, geom.type, pose, visual);
        });
    }

    // Агенты
    const currentAgentKeys = new Set();
    const agentLookup = {};
    if (data.agents) {
        document.getElementById('agent-count').textContent = data.agents.length;
        data.agents.forEach(agent => {
            const key = `agent_${agent.id}`;
            currentAgentKeys.add(key);
            agentLookup[agent.id] = agent;

            // Если агент выбран и перетаскивается (или только что отпущен) — не обновляем позицию
            const inGrace = selectedAgentId === agent.id &&
                (isDragging || (Date.now() - dragReleaseTime) < DRAG_GRACE_MS);
            if (inGrace) {
                if (!meshes[key]) {
                    updateOrCreateMesh(key, 'box', agent.pose, agent.visual);
                }
            } else {
                updateOrCreateMesh(key, 'box', agent.pose, agent.visual);
            }

            // TF frames: base_link агента
            updateTFFrameForAgent(key);

            // Линки кинематического дерева (URDF)
            if (agent.kinematic_frames && agent.kinematic_frames.length > 0) {
                let hasVisuals = false;
                agent.kinematic_frames.forEach(frame => {
                    // TF axes
                    const frameKey = `tf_${agent.id}_${frame.name}`;
                    currentAgentKeys.add(frameKey);
                    updateOrCreateKinematicFrame(frameKey, agent.id, frame.pose);

                    // Per-link mesh
                    if (frame.visual && frame.visual.type) {
                        hasVisuals = true;
                        const lmKey = `lm_${agent.id}_${frame.name}`;
                        currentAgentKeys.add(lmKey);
                        updateOrCreateLinkMesh(lmKey, frame.pose, frame.visual);
                    }
                });
                // Скрываем агент-коробку когда есть URDF-геометрия
                if (hasVisuals) {
                    agentHasUrdf[agent.id] = true;
                    if (meshes[key]) meshes[key].visible = false;
                }
            } else {
                // Нет URDF — показываем агент-коробку
                agentHasUrdf[agent.id] = false;
                if (meshes[key]) meshes[key].visible = true;
            }
        });
    }
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('agent_') && !currentAgentKeys.has(k)) removeMesh(k);
    });
    // Удаляем TF-frames для удалённых агентов/звеньев
    Object.keys(tfFrames).forEach(k => {
        if (k.startsWith('tf_') && !currentAgentKeys.has(k)) {
            scene.remove(tfFrames[k]);
            delete tfFrames[k];
        }
    });
    // Удаляем link meshes для удалённых агентов/звеньев
    Object.keys(linkMeshes).forEach(k => {
        if (!currentAgentKeys.has(k)) removeLinkMesh(k);
    });

    // Обновляем боковую панель
    if (selectedAgentId !== null && agentLookup[selectedAgentId]) {
        updateSidePanel(agentLookup[selectedAgentId]);
        const key = `agent_${selectedAgentId}`;
        if (meshes[key] && !transformControls.object) {
            selectedAgentMesh = meshes[key];
        }
    }

    // Пропы
    const currentPropKeys = new Set();
    if (data.props) {
        data.props.forEach(prop => {
            const key = `prop_${prop.id}`;
            currentPropKeys.add(key);
            const visualData = prop.visual || { type: 'box', size: [0.5, 0.5, 0.5], color: '#aaaaaa' };
            updateOrCreateMesh(key, 'box', prop.pose, visualData);
        });
    }
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('prop_') && !currentPropKeys.has(k)) removeMesh(k);
    });

    // Акторы
    const currentActorKeys = new Set();
    if (data.actors) {
        document.getElementById('actor-count').textContent = data.actors.length;
        data.actors.forEach(actor => {
            const key = `actor_${actor.id}`;
            currentActorKeys.add(key);
            const visualData = actor.visual || { type: 'box', size: [1, 2, 0.1], color: '#ffcc00' };
            updateOrCreateMesh(key, visualData.type || 'box', actor.pose, visualData);
        });
    }
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('actor_') && !currentActorKeys.has(k)) removeMesh(k);
    });

    // Зоны
    const currentZoneKeys = new Set();
    if (data.zones) {
        data.zones.forEach(zone => {
            const key = `zone_${zone.id}`;
            currentZoneKeys.add(key);
            if (zone.enabled) {
                const shape = zone.shape || {};
                let geomType = 'box';
                let size = [2, 0.05, 2];

                if (shape.shape_type === 'sphere') {
                    geomType = 'cylinder';
                    const r = shape.radius || 1;
                    size = [r * 2, 0.05, r * 2];
                } else if (shape.shape_type === 'aabb') {
                    size = shape.size || [2, 0.05, 2];
                }

                const center = shape.center || {};
                const pose = {
                    x: center.x || 0,
                    y: (center.z || 0) + 0.025,
                    z: -(center.y || 0),
                    yaw: 0
                };

                updateOrCreateMesh(key, geomType, pose,
                    { type: geomType, size: size, color: '#4488ff' },
                    { wireframe: true, opacity: 0.5, forceType: geomType }
                );
            } else {
                removeMesh(key);
            }
        });
    }
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('zone_') && !currentZoneKeys.has(k)) removeMesh(k);
    });
}

// ============================================================
// Команды на сервер
// ============================================================
function sendCommand(cmd) {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    fetch(`http://${host}:${port}/command?cmd=${cmd}`, { method: 'POST' })
        .then(r => r.json())
        .then(data => console.log('[Command]', data))
        .catch(err => console.error('[Command error]', err));
}

function sendMoveAgent(id, x, y, yaw) {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    const url = `http://${host}:${port}/command?cmd=move_agent&id=${id}&x=${x}&y=${y}&yaw=${yaw}`;
    fetch(url, { method: 'POST' })
        .then(r => r.json())
        .then(data => console.log('[Move agent]', data))
        .catch(err => console.error('[Move error]', err));
}

let gizmoVisible = true; // TransformControls axes visibility

// Скрыть/показать оси TransformControls
function toggleTransformControls() {
    gizmoVisible = !gizmoVisible;
    transformControls.visible = gizmoVisible;

    // Если оси скрыты — отключаем гизмо перемещения
    if (!gizmoVisible && transformControls.object) {
        transformControls.detach();
    }

    // Обновляем кнопку Mode — дизейблим если gizmo скрыт
    const modeBtn = document.getElementById('btn-transform');
    if (modeBtn) {
        modeBtn.disabled = !gizmoVisible;
        modeBtn.style.opacity = gizmoVisible ? '1' : '0.4';
    }

    const btn = document.getElementById('btn-gizmo');
    if (btn) {
        btn.textContent = gizmoVisible ? 'Axes: ON' : 'Axes: OFF';
    }
}

// Переключение режима трансформации (translate/rotate)
function toggleTransformMode() {
    if (transformMode === 'translate') {
        transformMode = 'rotate';
        transformControls.setMode('rotate');
        document.getElementById('btn-transform').textContent = 'Mode: Rotate';
    } else {
        transformMode = 'translate';
        transformControls.setMode('translate');
        document.getElementById('btn-transform').textContent = 'Mode: Translate';
    }
    transformControls.showX = true;
    transformControls.showY = true;
    transformControls.showZ = true;
}

// Следование камеры за агентом
function toggleFollow() {
    followMode = !followMode;
    const btn = document.getElementById('btn-follow');
    if (followMode) {
        btn.textContent = 'Unfollow';
        btn.classList.add('following');
        defaultCameraPosition.copy(camera.position);
        defaultCameraTarget.copy(controls.target);
        if (selectedAgentMesh) {
            controls.target.copy(selectedAgentMesh.position);
        }
    } else {
        btn.textContent = 'Follow';
        btn.classList.remove('following');
        followMode = false;
        camera.position.set(12, 12, 12);
        controls.target.set(0, 0, 0);
    }
}

// ============================================================
// Raycaster — клик по агенту
// ============================================================
const raycaster = new THREE.Raycaster();
const mouse = new THREE.Vector2();

renderer.domElement.addEventListener('click', (event) => {
    // Если недавно закончилось перетаскивание (менее 300ms) — игнорируем клик
    const timeSinceDrag = Date.now() - dragReleaseTime;
    if (timeSinceDrag < 300) {
        return;
    }

    mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
    mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;

    raycaster.setFromCamera(mouse, camera);

    const agentMeshList = Object.values(meshes).filter(m => m.userData.key && m.userData.key.startsWith('agent_') && m.visible);
    const linkMeshList = Object.values(linkMeshes);
    const intersects = raycaster.intersectObjects([...agentMeshList, ...linkMeshList]);

    if (intersects.length > 0) {
        const hit = intersects[0].object;
        let agentId;
        if (hit.userData.lmKey) {
            agentId = hit.userData.agentId;
        } else {
            agentId = parseInt(hit.userData.key.replace('agent_', ''));
        }
        // Для TransformControls всегда передаём агент-коробку (невидимую, но в сцене)
        const agentMesh = meshes[`agent_${agentId}`] || hit;
        selectAgent(agentId, agentMesh);
    } else {
        closeSidePanel();
    }
});

function selectAgent(agentId, mesh) {
    selectedAgentId = agentId;
    selectedAgentMesh = mesh;

    // Прикрепляем TransformControls только если gizmo видим
    if (gizmoVisible) {
        transformControls.attach(mesh);
        transformControls.setMode(transformMode);
        transformControls.showX = true;
        transformControls.showY = true;
        transformControls.showZ = true;
    }

    // Сбрасываем состояние аккордеона при смене агента
    pluginAccordionState = {};

    const agent = lastAgentData[agentId];
    if (agent) {
        updateSidePanel(agent);
    }
}

// ============================================================
// Server-Sent Events
// ============================================================
function connectSSE() {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    const url = `http://${host}:${port}/stream`;

    console.log(`Connecting to SSE: ${url}`);
    const evtSource = new EventSource(url);

    evtSource.onopen = () => {
        console.log('SSE connected');
        document.getElementById('conn-status').textContent = 'Connected';
        document.getElementById('conn-status').className = 'connected';
    };

    evtSource.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.agents) {
                data.agents.forEach(a => {
                    lastAgentData[a.id] = a;
                });
            }
            updateScene(data);
        } catch (e) {
            console.error('JSON parse error:', e);
        }
    };

    evtSource.onerror = (err) => {
        console.log('SSE error/disconnected');
        document.getElementById('conn-status').textContent = 'Disconnected';
        document.getElementById('conn-status').className = 'disconnected';
        evtSource.close();
        setTimeout(connectSSE, 2000);
    };
}

// ============================================================
// Render loop
// ============================================================
let frameCount = 0;
let lastFpsTime = performance.now();
let followCameraOffset = null;

function animate() {
    requestAnimationFrame(animate);

    if (followMode && selectedAgentMesh) {
        const pos = selectedAgentMesh.position;

        // Вычисляем желаемую позицию камеры с обновляемым offset
        if (!followCameraOffset) {
            followCameraOffset = camera.position.clone().sub(pos);
        }

        // Обновляем offset при вращении камеры пользователем
        // (сохраняем расстояние до цели, но используем новый угол обзора)
        const currentOffset = camera.position.clone().sub(pos);
        const dist = followCameraOffset.length();
        const newDist = currentOffset.length();
        // Плавно обновляем дистанцию, но сохраняем направление пользователя
        followCameraOffset.copy(currentOffset.normalize().multiplyScalar(
            THREE.MathUtils.lerp(followCameraOffset.length(), newDist, 0.02)
        ));

        // Прямое присваивание позиции цели без lerp — OrbitControls с damping сам сгладит
        controls.target.copy(pos);

        // Камера следует за целью с тем же offset
        camera.position.copy(pos.clone().add(followCameraOffset));
    } else {
        followCameraOffset = null;
    }

    controls.update();

    // Обновить TF frames каждый кадр
    updateAllTFFrames();
    renderer.render(scene, camera);

    frameCount++;
    const now = performance.now();
    if (now - lastFpsTime >= 1000) {
        document.getElementById('fps').textContent = frameCount;
        frameCount = 0;
        lastFpsTime = now;
    }
}

// ============================================================
// Plugin Input Form
// ============================================================
function showPluginInputForm(agentKey, pluginName) {
    const agentId = agentKey.replace('agent_', '');
    const formContainer = document.getElementById(`plugin-form-${agentId}-${pluginName}`);
    if (!formContainer) return;

    // Toggle visibility — при скрытии НЕ останавливаем интервал (робот продолжает ехать)
    if (formContainer.style.display === 'block') {
        formContainer.style.display = 'none';
        return;
    }

    const schema = pluginInputsSchemas[agentKey]?.[pluginName];
    if (!schema) return;

    const key = `${agentId}-${pluginName}`;
    const lastVals = pluginInputLastValues[key] || {};

    // Если все поля boolean — режим "только галочки" (без Send/Stop)
    const allBoolean = Object.values(schema).every(s => s.type === 'boolean');

    let html = `<div class="plugin-input-form-inner" data-agent="${agentId}" data-plugin="${pluginName}">`;
    for (const [fieldName, fieldSchema] of Object.entries(schema)) {
        const label = fieldSchema.label || (fieldSchema.unit ? `${fieldName} (${fieldSchema.unit})` : fieldName);
        const val = lastVals[fieldName] !== undefined ? lastVals[fieldName]
                  : (fieldSchema.default !== undefined ? fieldSchema.default : 0);

        if (fieldSchema.type === 'boolean') {
            const checked = val ? 'checked' : '';
            const onchange = allBoolean
                ? `onchange="sendBooleanPluginField('${agentId}', '${pluginName}', '${fieldName}', this.checked)"`
                : '';
            html += `
                <div class="form-field">
                    <label>${escapeHtml(label)}</label>
                    <input type="checkbox"
                           id="input-${agentId}-${pluginName}-${fieldName}"
                           ${checked} ${onchange} />
                </div>`;
        } else {
            const min = fieldSchema.min !== undefined ? fieldSchema.min : '';
            const max = fieldSchema.max !== undefined ? fieldSchema.max : '';
            html += `
                <div class="form-field">
                    <label>${escapeHtml(label)}</label>
                    <input type="number"
                           id="input-${agentId}-${pluginName}-${fieldName}"
                           value="${val}"
                           ${min !== '' ? `min="${min}"` : ''}
                           ${max !== '' ? `max="${max}"` : ''}
                           step="0.1" />
                </div>`;
        }
    }
    if (!allBoolean) {
        html += `<button class="plugin-send-btn" onclick="startPluginInput('${agentId}', '${pluginName}')">&#9654; Send</button>`;
        html += `<button class="plugin-stop-btn" onclick="stopPluginInput('${agentId}', '${pluginName}')">&#9632; Stop</button>`;
    }
    html += `</div>`;

    formContainer.innerHTML = html;
    formContainer.style.display = 'block';
}

// Мгновенная отправка одного boolean-поля (для checkbox onchange)
function sendBooleanPluginField(agentId, pluginName, fieldName, checked) {
    const schema = pluginInputsSchemas[`agent_${agentId}`]?.[pluginName];
    if (!schema) return;
    // Собираем текущие значения всех полей из DOM, меняем нужное
    const values = {};
    for (const [fn, fs] of Object.entries(schema)) {
        const input = document.getElementById(`input-${agentId}-${pluginName}-${fn}`);
        values[fn] = fn === fieldName ? checked : (input ? input.checked : (fs.default !== undefined ? fs.default : false));
    }
    const key = `${agentId}-${pluginName}`;
    pluginInputLastValues[key] = { ...values };
    _sendValues(agentId, pluginName, values);
}

// Отправить значения напрямую (без чтения DOM)
function _sendValues(agentId, pluginName, values) {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    const body = JSON.stringify(values);
    const url = `http://${host}:${port}/command?cmd=plugin_input&agent_id=${agentId}&plugin=${pluginName}&body=${encodeURIComponent(body)}`;
    fetch(url, { method: 'POST' }).catch(() => {});
}

// Остановить интервал и немедленно отправить нули
function stopPluginInput(agentId, pluginName) {
    const key = `${agentId}-${pluginName}`;
    if (pluginInputIntervals[key]) {
        clearInterval(pluginInputIntervals[key]);
        delete pluginInputIntervals[key];
    }
    delete pluginInputLastValues[key];
    const schema = pluginInputsSchemas[`agent_${agentId}`]?.[pluginName];
    if (schema) {
        const zeros = {};
        for (const [fieldName, fieldSchema] of Object.entries(schema)) {
            zeros[fieldName] = fieldSchema.type === 'boolean'
                ? (fieldSchema.default !== undefined ? fieldSchema.default : false)
                : 0;
        }
        _sendValues(agentId, pluginName, zeros);
    }
}

// Начать непрерывную отправку (перезапускает если уже запущен с новыми значениями)
function startPluginInput(agentId, pluginName) {
    const key = `${agentId}-${pluginName}`;
    if (pluginInputIntervals[key]) {
        clearInterval(pluginInputIntervals[key]);
        delete pluginInputIntervals[key];
    }
    const schema = pluginInputsSchemas[`agent_${agentId}`]?.[pluginName];
    if (!schema) return;
    // Захватываем значения из формы прямо сейчас (closure — не зависит от DOM в будущем)
    const values = {};
    for (const [fieldName, fieldSchema] of Object.entries(schema)) {
        const input = document.getElementById(`input-${agentId}-${pluginName}-${fieldName}`);
        if (fieldSchema.type === 'boolean') {
            values[fieldName] = input ? input.checked : (fieldSchema.default !== undefined ? fieldSchema.default : false);
        } else {
            values[fieldName] = input ? parseFloat(input.value) : 0;
        }
    }
    pluginInputLastValues[key] = { ...values };
    _sendValues(agentId, pluginName, values);
    pluginInputIntervals[key] = setInterval(() => _sendValues(agentId, pluginName, values), 50);
}

// Экспортируем функции в глобальную область для onclick
window.sendCommand = sendCommand;
window.toggleFollow = toggleFollow;
window.closeSidePanel = closeSidePanel;
window.toggleTransformMode = toggleTransformMode;
window.toggleTransformControls = toggleTransformControls;
window.showPluginInputForm = showPluginInputForm;
window.sendBooleanPluginField = sendBooleanPluginField;
window.startPluginInput = startPluginInput;
window.stopPluginInput = stopPluginInput;

// Запуск
connectSSE();
animate();

// ============================================================
// Resize
// ============================================================
window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
});