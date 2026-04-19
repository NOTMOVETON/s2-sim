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

// Лидар: точки попаданий (Points-объекты Three.js, ключ: agentKey)
const lidarPointObjects = {};

// Коллизии: полупрозрачные шейпы для каждого агента (ключ: agentKey)
const collisionMeshes = {};
let collisionsVisible = false;   // управляется кнопкой "Collisions"

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
controls.mouseButtons = {
    LEFT:   THREE.MOUSE.ROTATE,
    MIDDLE: null,
    RIGHT:  THREE.MOUSE.PAN,
};

// Shift+LMB по пустому месту → ручной pan (capture phase — до OrbitControls)
renderer.domElement.addEventListener('mousedown', e => {
    if (e.button === 0 && e.shiftKey) {
        const rect = renderer.domElement.getBoundingClientRect();
        const mx = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        const my = -((e.clientY - rect.top) / rect.height) * 2 + 1;
        const tempRay = new THREE.Raycaster();
        tempRay.setFromCamera(new THREE.Vector2(mx, my), camera);
        const staticMeshList = Object.entries(meshes)
            .filter(([k]) => k.startsWith('static_'))
            .map(([, m]) => m);
        if (tempRay.intersectObjects(staticMeshList).length === 0) {
            e.stopImmediatePropagation();
            controls.enabled = false;
            startManualPan(e);
        }
    }
}, true);

renderer.domElement.addEventListener('mouseup', () => {
    if (manualPanning) {
        stopManualPan();
        controls.enabled = true;
    }
});

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

transformControls.addEventListener('mouseDown', function () {
    if (editorMode && selectedPrimitiveId !== null) {
        pushUndoSnapshot();
    }
});

transformControls.addEventListener('mouseUp', function () {
    if (editorMode && selectedPrimitiveId !== null) {
        // Синхронизируем позу и размеры из меша в editorPrimitives, затем отправляем на сервер
        syncPrimitiveFromMesh(selectedPrimitiveId);
        sendGeometryToServer();
    } else if (selectedAgentId !== null && selectedAgentMesh) {
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
            // Sim: X=ширина, Y=глубина, Z=высота
            // Three.js BoxGeometry(width, height, depth): height — вдоль Y (= sim Z), depth — вдоль Z (= sim Y)
            return new THREE.BoxGeometry(
                size.x !== undefined ? size.x : 1,
                size.z !== undefined ? size.z : 1,  // sim Z → Three.js Y (высота)
                size.y !== undefined ? size.y : 1   // sim Y → Three.js Z (глубина)
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
        // DoubleSide используется для примитивов редактора сцены: при некоторых
        // вращениях или конвертации осей (Z-up → Y-up) нормали могут оказаться
        // перевёрнутыми, из-за чего видны только задние стенки. DoubleSide
        // устраняет этот эффект без изменения геометрии или матриц трансформации.
        const material = new THREE.MeshStandardMaterial({
            color: hexToColor(visual?.color),
            transparent: opts.wireframe || opts.transparent || false,
            opacity: opts.opacity !== undefined ? opts.opacity : 1.0,
            wireframe: opts.wireframe || false,
            side: opts.doubleSide ? THREE.DoubleSide : THREE.FrontSide,
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
    mesh.rotation.set(pose.roll || 0, pose.yaw || 0, -(pose.pitch || 0), 'YZX');

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
// Toast уведомления
// ============================================================
function showToast(message, duration = 3000) {
    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.textContent = message;
    document.body.appendChild(toast);
    setTimeout(() => {
        toast.classList.add('fade-out');
        setTimeout(() => { if (toast.parentNode) toast.parentNode.removeChild(toast); }, 500);
    }, duration);
}

// ============================================================
// Редактор сцены — состояние
// ============================================================
let editorMode = false;
let editorPrimitives = [];           // { id, type, pose, size, radius, height, color }
let selectedPrimitiveId = null;
let nextPrimitiveId = 0;
let editorTransformMode = 'translate';
let staticGeometryData = [];         // кеш последних данных geometry от сервера

// Редактор агентов — состояние
let editorAgents = [];               // { localId, name, domain_id, pose, visual, urdf, plugins }
let nextAgentLocalId = 0;
let placingAgentMode = false;        // ожидаем клик в сцену для позиционирования агента
let editingAgentLocalId = null;      // localId редактируемого агента (null = новый)
let pluginRegistry = [];             // кеш реестра плагинов с сервера
let urdfList = [];                   // кеш списка URDF-файлов
let activeEditorTab = 'geometry';    // 'geometry' | 'agents'
let _newAgentFormListener = null;    // ссылка на input-listener формы нового агента

// Edge-snapping — состояние
let snapEdge1 = null;         // { primId, edgeMid: THREE.Vector3 }
let snapEdge2 = null;
let edgeHighlightMesh = null; // подсветка выбранного ребра
let shiftHeld = false;

// Undo-стек
const undoStack = [];
const MAX_UNDO = 50;

// Copy/paste буфер
let clipboardPrimitives = [];

// Ручной pan (Shift+LMB по пустому месту)
let manualPanning = false;
let panStartMouse = new THREE.Vector2();
let panStartTarget = new THREE.Vector3();
let panStartCameraPos = new THREE.Vector3();

// Плоскость Y=0 (= Z=0 в симуляторе) для raycast при размещении агента
const groundPlane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);

// ============================================================
// Редактор сцены — вспомогательные функции
// ============================================================

/**
 * Найти ближайшее ребро примитива к точке попадания (мировые координаты).
 * Возвращает { worldMid, worldDir, halfLen, shape } или null.
 *   shape='segment' — отрезок (рёбра box)
 *   shape='ring'    — окружность (торцы cylinder)
 *   shape='point'   — точка (sphere)
 */
function findNearestEdge(prim, mesh, hitPointWorld) {
    const localHit = hitPointWorld.clone()
        .applyMatrix4(mesh.matrixWorld.clone().invert());

    let bestLocal = null;
    let bestDir   = new THREE.Vector3(1, 0, 0);
    let bestHalf  = 0;
    let bestShape = 'segment';

    if (prim.type === 'box') {
        const hx = (prim.size?.x || 1) / 2;
        const hy = (prim.size?.z || 1) / 2;   // Three.js Y = sim Z
        const hz = (prim.size?.y || 1) / 2;   // Three.js Z = sim Y
        // 12 рёбер: 4 параллельных каждой из осей X, Y, Z
        const candidates = [];
        for (const sy of [-1, 1]) for (const sz of [-1, 1])
            candidates.push({ mid: new THREE.Vector3(0, sy*hy, sz*hz), dir: new THREE.Vector3(1,0,0), half: hx });
        for (const sx of [-1, 1]) for (const sz of [-1, 1])
            candidates.push({ mid: new THREE.Vector3(sx*hx, 0, sz*hz), dir: new THREE.Vector3(0,1,0), half: hy });
        for (const sx of [-1, 1]) for (const sy of [-1, 1])
            candidates.push({ mid: new THREE.Vector3(sx*hx, sy*hy, 0), dir: new THREE.Vector3(0,0,1), half: hz });

        let bestDist = Infinity;
        for (const c of candidates) {
            const d = c.mid.distanceTo(localHit);
            if (d < bestDist) {
                bestDist  = d;
                bestLocal = c.mid.clone();
                bestDir   = c.dir.clone();
                bestHalf  = c.half;
            }
        }
        bestShape = 'segment';
    } else if (prim.type === 'cylinder') {
        // CylinderGeometry: вертикальная ось — локальная Y
        const h = (prim.height || 1.0) / 2;
        const r = prim.radius || 0.5;
        const topMid = new THREE.Vector3(0,  h, 0);
        const botMid = new THREE.Vector3(0, -h, 0);
        bestLocal = topMid.distanceTo(localHit) <= botMid.distanceTo(localHit)
            ? topMid.clone() : botMid.clone();
        bestDir   = new THREE.Vector3(0, 1, 0); // ось цилиндра
        bestHalf  = r;
        bestShape = 'ring';
    } else if (prim.type === 'sphere') {
        const r = prim.radius || 0.5;
        bestLocal = localHit.clone().normalize().multiplyScalar(r);
        bestDir   = new THREE.Vector3(0, 1, 0);
        bestHalf  = 0;
        bestShape = 'point';
    }

    if (!bestLocal) return null;

    const worldMid = bestLocal.clone().applyMatrix4(mesh.matrixWorld);
    const worldDir = bestDir.clone().transformDirection(mesh.matrixWorld).normalize();
    return { worldMid, worldDir, halfLen: bestHalf, shape: bestShape };
}

/**
 * Показать жёлтую подсветку выбранного ребра.
 *   shape='segment': тонкий цилиндр вдоль ребра (box).
 *   shape='ring':    тор (торцевое ребро cylinder).
 *   shape='point':   маленькая сфера (sphere).
 */
function showEdgeHighlight(edgeInfo) {
    if (edgeHighlightMesh) {
        scene.remove(edgeHighlightMesh);
        edgeHighlightMesh.geometry?.dispose();
        edgeHighlightMesh.material?.dispose();
        edgeHighlightMesh = null;
    }
    const mat = new THREE.MeshBasicMaterial({ color: 0xFFFF00, depthTest: false });
    const { worldMid, worldDir, halfLen, shape } = edgeInfo;

    if (shape === 'segment') {
        // CylinderGeometry ориентирован по локальной Y; выровнять по worldDir
        const geo = new THREE.CylinderGeometry(0.03, 0.03, halfLen * 2, 8);
        edgeHighlightMesh = new THREE.Mesh(geo, mat);
        edgeHighlightMesh.position.copy(worldMid);
        const up = new THREE.Vector3(0, 1, 0);
        if (Math.abs(worldDir.dot(up)) < 0.9999) {
            edgeHighlightMesh.quaternion.setFromUnitVectors(up, worldDir);
        }
    } else if (shape === 'ring') {
        // TorusGeometry лежит в плоскости XY (ось тора = Z); повернуть к worldDir
        const geo = new THREE.TorusGeometry(halfLen, 0.03, 8, 32);
        edgeHighlightMesh = new THREE.Mesh(geo, mat);
        edgeHighlightMesh.position.copy(worldMid);
        const zAxis = new THREE.Vector3(0, 0, 1);
        if (Math.abs(worldDir.dot(zAxis)) < 0.9999) {
            edgeHighlightMesh.quaternion.setFromUnitVectors(zAxis, worldDir);
        }
    } else {
        // point — маленькая сфера
        const geo = new THREE.SphereGeometry(0.08, 8, 8);
        edgeHighlightMesh = new THREE.Mesh(geo, mat);
        edgeHighlightMesh.position.copy(worldMid);
    }

    scene.add(edgeHighlightMesh);
}

/**
 * Сбросить состояние edge-snapping и убрать подсветку.
 */
function clearEdgeSnap() {
    snapEdge1 = null;
    snapEdge2 = null;
    if (edgeHighlightMesh) {
        scene.remove(edgeHighlightMesh);
        edgeHighlightMesh.geometry?.dispose();
        edgeHighlightMesh.material?.dispose();
        edgeHighlightMesh = null;
    }
}

// ============================================================
// Pan вручную (Shift+LMB по пустому месту)
// ============================================================

function startManualPan(e) {
    manualPanning = true;
    panStartMouse.set(e.clientX, e.clientY);
    panStartTarget.copy(controls.target);
    panStartCameraPos.copy(camera.position);
}

function updateManualPan(e) {
    if (!manualPanning) return;
    const dx = (e.clientX - panStartMouse.x) / window.innerWidth;
    const dy = (e.clientY - panStartMouse.y) / window.innerHeight;
    const right = new THREE.Vector3();
    right.crossVectors(camera.getWorldDirection(new THREE.Vector3()), camera.up).normalize();
    const up = camera.up.clone();
    const panScale = camera.position.distanceTo(controls.target) * 1.0;
    const panDelta = right.multiplyScalar(-dx * panScale)
                         .add(up.multiplyScalar(dy * panScale));
    controls.target.copy(panStartTarget).add(panDelta);
    camera.position.copy(panStartCameraPos).add(panDelta);
    controls.update();
}

function stopManualPan() {
    manualPanning = false;
}

// ============================================================
// Undo
// ============================================================

function pushUndoSnapshot() {
    const snapshot = JSON.parse(JSON.stringify(editorPrimitives));
    undoStack.push(snapshot);
    if (undoStack.length > MAX_UNDO) undoStack.shift();
}

function undo() {
    if (undoStack.length === 0) return;
    const snapshot = undoStack.pop();
    Object.keys(meshes).filter(k => k.startsWith('static_')).forEach(k => removeMesh(k));
    editorPrimitives = snapshot;
    editorPrimitives.forEach(p => createPrimitiveMesh(p));
    transformControls.detach();
    selectedPrimitiveId = null;
    document.getElementById('primitive-props').style.display = 'none';
    sendGeometryToServer();
}

// ============================================================
// Copy / Paste / Delete
// ============================================================

function copySelected() {
    if (!selectedPrimitiveId) return;
    const prim = editorPrimitives.find(p => p.id === selectedPrimitiveId);
    if (prim) clipboardPrimitives = [JSON.parse(JSON.stringify(prim))];
}

function pasteSelected() {
    if (clipboardPrimitives.length === 0) return;
    pushUndoSnapshot();
    selectedPrimitiveId = null;
    transformControls.detach();
    const PASTE_OFFSET = 0.5;
    clipboardPrimitives.forEach(orig => {
        const copy = JSON.parse(JSON.stringify(orig));
        copy.id = `prim_${nextPrimitiveId++}`;
        copy.pose.x += PASTE_OFFSET;
        copy.pose.y += PASTE_OFFSET;
        editorPrimitives.push(copy);
        createPrimitiveMesh(copy);
    });
    const last = editorPrimitives[editorPrimitives.length - 1];
    if (last) selectPrimitive(last.id);
    sendGeometryToServer();
}

function deleteSelected() {
    if (!selectedPrimitiveId) return;
    pushUndoSnapshot();
    editorPrimitives = editorPrimitives.filter(p => p.id !== selectedPrimitiveId);
    removeMesh(`static_${selectedPrimitiveId}`);
    selectedPrimitiveId = null;
    transformControls.detach();
    document.getElementById('primitive-props').style.display = 'none';
    sendGeometryToServer();
}

/**
 * Обработчик Shift+ЛКМ для edge-snapping.
 * Первый клик — запомнить ребро 1 и подсветить.
 * Второй клик — выполнить snap (переместить примитив 2 так, чтобы рёбра совпали).
 * @param {MouseEvent} event
 */
function handleEdgeSnapClick(event) {
    mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
    mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;
    raycaster.setFromCamera(mouse, camera);

    const staticMeshList = Object.entries(meshes)
        .filter(([k]) => k.startsWith('static_'))
        .map(([, m]) => m);
    const hits = raycaster.intersectObjects(staticMeshList);
    if (hits.length === 0) {
        clearEdgeSnap();
        return;
    }

    const hit = hits[0];
    const primId = hit.object.userData.key.replace('static_', '');
    const mesh = hit.object;
    const prim = editorPrimitives.find(p => p.id === primId);
    if (!prim) return;

    const edgeInfo = findNearestEdge(prim, mesh, hit.point);
    if (!edgeInfo) return;

    if (!snapEdge1) {
        // Первый клик — запомнить ребро и показать подсветку
        snapEdge1 = { primId, edgeMid: edgeInfo.worldMid };
        showEdgeHighlight(edgeInfo);
    } else {
        // Второй клик
        if (snapEdge1.primId === primId) {
            // Повторный клик на тот же примитив — сбросить
            clearEdgeSnap();
            return;
        }

        const mesh2 = meshes[`static_${primId}`];
        if (!mesh2) { clearEdgeSnap(); return; }

        // Переместить примитив 2 так, чтобы edgeMid2 совпал с edgeMid1
        const delta = snapEdge1.edgeMid.clone().sub(edgeInfo.worldMid);
        mesh2.position.add(delta);

        // Синхронизировать pose и отправить на сервер
        clearEdgeSnap();
        syncPrimitiveFromMesh(primId);
        sendGeometryToServer();
    }
}

/** Создать или обновить меш для примитива редактора */
function createPrimitiveMesh(prim) {
    const pose = prim.pose;
    const visual = {
        type:   prim.type,
        size:   [prim.size?.x || 1, prim.size?.y || 1, prim.size?.z || 1],
        color:  prim.color  || '#808080',
        radius: prim.radius || 0.5,
        height: prim.height || 1.0,
    };
    // Удаляем старый меш если есть (при recreate)
    removeMesh(`static_${prim.id}`);
    // doubleSide: true — чтобы примитивы были видны с обеих сторон независимо
    // от ориентации нормалей (возникает при конвертации координат Z-up → Y-up)
    updateOrCreateMesh(`static_${prim.id}`, prim.type, pose, visual, { doubleSide: true });
}

/** Пересоздать меш с обновлёнными размерами (после scale) */
function recreatePrimitiveMesh(prim) {
    createPrimitiveMesh(prim);
    // Если примитив был выбран — переприкрепляем TransformControls
    if (selectedPrimitiveId === prim.id) {
        const mesh = meshes[`static_${prim.id}`];
        if (mesh) {
            transformControls.attach(mesh);
        }
    }
}

/** Синхронизировать pozу и размеры из меша в editorPrimitives */
function syncPrimitiveFromMesh(id) {
    const prim = editorPrimitives.find(p => p.id === id);
    const mesh = meshes[`static_${id}`];
    if (!prim || !mesh) return;

    // Позиция (Three.js Y-up → sim Z-up)
    prim.pose.x = mesh.position.x;
    prim.pose.y = -mesh.position.z;
    prim.pose.z = mesh.position.y;
    prim.pose.yaw   =  mesh.rotation.y;
    prim.pose.pitch = -mesh.rotation.z;
    prim.pose.roll  =  mesh.rotation.x;

    // Размеры (с учётом накопленного scale)
    // BoxGeometry создаётся как (size.x, size.z, size.y), поэтому:
    //   mesh.scale.x → sim size.x
    //   mesh.scale.y → sim size.z  (Three.js Y = sim Z)
    //   mesh.scale.z → sim size.y  (Three.js Z = sim Y)
    if (prim.type === 'box') {
        prim.size.x = (prim.size.x || 1) * mesh.scale.x;
        prim.size.z = (prim.size.z || 1) * mesh.scale.y;  // Three.js Y scale → sim Z
        prim.size.y = (prim.size.y || 1) * mesh.scale.z;  // Three.js Z scale → sim Y
        mesh.scale.set(1, 1, 1);
        recreatePrimitiveMesh(prim);
    } else if (prim.type === 'cylinder') {
        prim.radius = (prim.radius || 0.5) * Math.max(mesh.scale.x, mesh.scale.z);
        prim.height = (prim.height || 1.0) * mesh.scale.y;
        mesh.scale.set(1, 1, 1);
        recreatePrimitiveMesh(prim);
    } else if (prim.type === 'sphere') {
        prim.radius = (prim.radius || 0.5) * Math.max(mesh.scale.x, mesh.scale.y, mesh.scale.z);
        mesh.scale.set(1, 1, 1);
        recreatePrimitiveMesh(prim);
    }

    updatePrimitivePropsPanel(id);
}

/** Обновить панель свойств примитива */
function updatePrimitivePropsPanel(id) {
    const prim = editorPrimitives.find(p => p.id === id);
    if (!prim) {
        document.getElementById('primitive-props').style.display = 'none';
        return;
    }
    document.getElementById('primitive-props').style.display = 'block';
    document.getElementById('primitive-type-label').textContent =
        prim.type.charAt(0).toUpperCase() + prim.type.slice(1);

    document.getElementById('prop-color').value = prim.color || '#808080';

    // Показываем нужные поля
    document.getElementById('props-box').style.display      = prim.type === 'box'      ? '' : 'none';
    document.getElementById('props-cylinder').style.display = prim.type === 'cylinder' ? '' : 'none';
    document.getElementById('props-sphere').style.display   = prim.type === 'sphere'   ? '' : 'none';

    if (prim.type === 'box') {
        document.getElementById('prop-sx').value = (prim.size?.x || 1).toFixed(2);
        document.getElementById('prop-sy').value = (prim.size?.y || 1).toFixed(2);
        document.getElementById('prop-sz').value = (prim.size?.z || 1).toFixed(2);
    } else if (prim.type === 'cylinder') {
        document.getElementById('prop-radius').value = (prim.radius || 0.5).toFixed(2);
        document.getElementById('prop-height').value = (prim.height || 1.0).toFixed(2);
    } else if (prim.type === 'sphere') {
        document.getElementById('prop-radius-sphere').value = (prim.radius || 0.5).toFixed(2);
    }
}

/** Выбрать примитив по ID */
function selectPrimitive(id) {
    selectedPrimitiveId = id;
    // Снять выбор агента если был выбран
    selectedAgentId = null;
    selectedAgentMesh = null;

    const mesh = meshes[`static_${id}`];
    if (mesh) {
        transformControls.attach(mesh);
        transformControls.setMode(editorTransformMode);
        transformControls.showX = true;
        transformControls.showY = true;
        transformControls.showZ = true;
    }
    updatePrimitivePropsPanel(id);
}

/** Добавить новый примитив в центр сцены */
function addPrimitive(type) {
    pushUndoSnapshot();
    const id = `prim_${nextPrimitiveId++}`;
    const prim = {
        id,
        type,
        pose: { x: 0, y: 0, z: 0.5, yaw: 0, pitch: 0, roll: 0 },
        size: { x: 1, y: 1, z: 1 },
        radius: 0.5,
        height: 1.0,
        color: '#808080',
    };
    editorPrimitives.push(prim);
    createPrimitiveMesh(prim);
    selectPrimitive(id);
    sendGeometryToServer();
}

/** Удалить примитив по ID */
function deletePrimitive(id) {
    if (!id) return;
    pushUndoSnapshot();
    editorPrimitives = editorPrimitives.filter(p => p.id !== id);
    removeMesh(`static_${id}`);
    transformControls.detach();
    selectedPrimitiveId = null;
    document.getElementById('primitive-props').style.display = 'none';
    sendGeometryToServer();
}

/** Обновить цвет выбранного примитива немедленно */
window.onPrimColorChange = function(value) {
    if (!selectedPrimitiveId) return;
    pushUndoSnapshot();
    const prim = editorPrimitives.find(p => p.id === selectedPrimitiveId);
    if (!prim) return;
    prim.color = value;
    const mesh = meshes[`static_${selectedPrimitiveId}`];
    if (mesh) mesh.material.color.setStyle(value);
    // Не отправляем на сервер сразу — ждём явного «Применить» или следующего mouseUp
};

/** Обновить размеры выбранного примитива из полей ввода */
window.onPrimSizeChange = function() {
    if (!selectedPrimitiveId) return;
    pushUndoSnapshot();
    const prim = editorPrimitives.find(p => p.id === selectedPrimitiveId);
    if (!prim) return;

    if (prim.type === 'box') {
        prim.size.x = parseFloat(document.getElementById('prop-sx').value) || 1;
        prim.size.y = parseFloat(document.getElementById('prop-sy').value) || 1;
        prim.size.z = parseFloat(document.getElementById('prop-sz').value) || 1;
    } else if (prim.type === 'cylinder') {
        prim.radius = parseFloat(document.getElementById('prop-radius').value) || 0.5;
        prim.height = parseFloat(document.getElementById('prop-height').value) || 1.0;
    } else if (prim.type === 'sphere') {
        prim.radius = parseFloat(document.getElementById('prop-radius-sphere').value) || 0.5;
    }
    recreatePrimitiveMesh(prim);
};

/** Переключить режим трансформации редактора */
window.setEditorTransformMode = function(mode) {
    editorTransformMode = mode;
    transformControls.setMode(mode);
    document.getElementById('btn-mode-translate').classList.toggle('active', mode === 'translate');
    document.getElementById('btn-mode-rotate').classList.toggle('active', mode === 'rotate');
    document.getElementById('btn-mode-scale').classList.toggle('active', mode === 'scale');
};

/** Отправить текущую геометрию на сервер (применить) */
window.sendGeometryToServer = function sendGeometryToServer() {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    const payload = {
        geometry: editorPrimitives.map(p => ({
            type:   p.type,
            x:      p.pose.x, y: p.pose.y, z: p.pose.z,
            yaw:    p.pose.yaw, pitch: p.pose.pitch, roll: p.pose.roll,
            sx:     p.size?.x || 1, sy: p.size?.y || 1, sz: p.size?.z || 1,
            radius: p.radius || 0.5,
            height: p.height || 1.0,
            color:  p.color || '#808080',
        }))
    };
    fetch(`http://${host}:${port}/api/scene/geometry`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
    }).catch(err => console.error('[Geometry update error]', err));
};

/** Кнопка «Применить» — синхронизировать выбранный примитив и отправить */
window.applyGeometry = function() {
    if (selectedPrimitiveId) {
        syncPrimitiveFromMesh(selectedPrimitiveId);
    }
    sendGeometryToServer();
    showToast('Геометрия применена');
};

/** Сохранить сцену в YAML */
window.saveScene = async function() {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    try {
        // Сначала сохраняем агентов (если вкладка агентов активна или есть изменения)
        if (editorAgents.length > 0) {
            await sendAgentsToServer();
        }
        const r = await fetch(`http://${host}:${port}/api/scene/save`, { method: 'POST' });
        const d = await r.json();
        if (d.ok) showToast(`Сцена сохранена: ${d.path}`);
        else showToast(`Ошибка сохранения: ${d.error}`);
    } catch (err) {
        showToast(`Ошибка: ${err.message}`);
    }
};

/** Войти в режим редактора сцены */
function enterEditorMode() {
    editorMode = true;

    // Скрыть боковую панель агента
    closeSidePanel();

    // Показать редактор
    document.getElementById('editor-panel').style.display = 'block';
    document.getElementById('btn-edit-scene').classList.add('active');

    // Ярче сетка
    gridHelper.material.opacity = 1.0;
    gridHelper.material.transparent = false;

    // Инициализируем editorPrimitives из кеша геометрии сервера
    editorPrimitives = staticGeometryData.map((geom, i) => ({
        id: `prim_${i}`,
        type:   geom.type   || 'box',
        pose:   { x: geom.x || 0, y: geom.y || 0, z: geom.z || 0,
                  yaw: geom.yaw || 0, pitch: geom.pitch || 0, roll: geom.roll || 0 },
        size:   { x: geom.sx || 1, y: geom.sy || 1, z: geom.sz || 1 },
        radius: geom.radius || 0.5,
        height: geom.height || 1.0,
        color:  geom.color  || '#808080',
    }));
    nextPrimitiveId = editorPrimitives.length;
    undoStack.length = 0;
    clipboardPrimitives = [];

    // Удаляем static_N меши и создаём static_prim_N
    Object.keys(meshes).forEach(k => { if (k.startsWith('static_')) removeMesh(k); });
    editorPrimitives.forEach(prim => createPrimitiveMesh(prim));

    // Устанавливаем режим трансформации
    transformControls.setMode(editorTransformMode);

    // Предзагружаем реестр плагинов и список URDF (нужно для вкладки агентов)
    fetchPluginRegistry();
    fetchUrdfList();
}

/** Выйти из режима редактора сцены */
function exitEditorMode() {
    // Отправить финальное состояние на сервер
    if (selectedPrimitiveId) syncPrimitiveFromMesh(selectedPrimitiveId);
    sendGeometryToServer();

    editorMode = false;
    transformControls.detach();
    selectedPrimitiveId = null;
    document.getElementById('editor-panel').style.display = 'none';
    document.getElementById('primitive-props').style.display = 'none';
    document.getElementById('btn-edit-scene').classList.remove('active');

    // Затемнить сетку
    gridHelper.material.opacity = 0.6;
    gridHelper.material.transparent = true;

    // Пересоздать static_N меши из editorPrimitives (индексированные)
    Object.keys(meshes).forEach(k => { if (k.startsWith('static_')) removeMesh(k); });
    editorPrimitives.forEach((prim, i) => {
        const visual = {
            type:   prim.type,
            size:   [prim.size?.x || 1, prim.size?.y || 1, prim.size?.z || 1],
            color:  prim.color  || '#808080',
            radius: prim.radius || 0.5,
            height: prim.height || 1.0,
        };
        updateOrCreateMesh(`static_${i}`, prim.type, prim.pose, visual, { doubleSide: true });
    });
    editorPrimitives = [];

    // Восстановить режим агентов
    transformControls.setMode(transformMode);

    // Очистить состояние редактора агентов
    cancelPlaceAgent();
    window.closeAgentForm();
    // Удалить все preview-меши агентов
    Object.keys(meshes).forEach(k => { if (k.startsWith('agent_edit_')) removeMesh(k); });
    editorAgents = [];
    editingAgentLocalId = null;
    // Сбросить вкладку на геометрию при следующем открытии
    activeEditorTab = 'geometry';
}

/** Переключить режим редактора */
window.toggleEditorMode = function() {
    if (editorMode) exitEditorMode();
    else            enterEditorMode();
};

// Обёртки для вызова из HTML (module scope не виден в onclick)
window.addPrimitiveBox      = () => addPrimitive('box');
window.addPrimitiveCylinder = () => addPrimitive('cylinder');
window.addPrimitiveSphere   = () => addPrimitive('sphere');
window.deleteSelectedPrimitive = () => deleteSelected();

// ============================================================
// ============================================================
// Лидар: точки попаданий
// ============================================================

function renderLidarPoints(agentKey, data) {
    // Удаляем старый объект если есть
    if (lidarPointObjects[agentKey]) {
        scene.remove(lidarPointObjects[agentKey]);
        lidarPointObjects[agentKey].geometry.dispose();
        lidarPointObjects[agentKey].material.dispose();
        delete lidarPointObjects[agentKey];
    }

    if (!data.visible || !data.points || data.points.length === 0) return;

    // Координаты: симулятор (x=East, y=North, z=Up) → Three.js (x=East, y=Up, z=-North)
    const positions = new Float32Array(data.points.length * 3);
    for (let i = 0; i < data.points.length; i++) {
        const p = data.points[i];
        positions[i * 3 + 0] =  p[0];
        positions[i * 3 + 1] =  p[2];
        positions[i * 3 + 2] = -p[1];
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const material = new THREE.PointsMaterial({
        color: data.color || '#00FFFF',
        size: 0.08,
    });

    const pts = new THREE.Points(geometry, material);
    scene.add(pts);
    lidarPointObjects[agentKey] = pts;
}

// ============================================================
// Коллизии: полупрозрачные шейпы агентов
// ============================================================

function updateCollisionMesh(agentKey, agent) {
    // Удаляем старый шейп
    if (collisionMeshes[agentKey]) {
        scene.remove(collisionMeshes[agentKey]);
        collisionMeshes[agentKey].geometry.dispose();
        collisionMeshes[agentKey].material.dispose();
        delete collisionMeshes[agentKey];
    }

    if (!collisionsVisible || !agent.has_collision || !agent.bounding) return;

    const b = agent.bounding;
    let geometry;
    if (b.type === 'sphere') {
        geometry = new THREE.SphereGeometry(b.radius || 0.5, 16, 12);
    } else {
        // box: sx/sy/sz — полные размеры
        geometry = new THREE.BoxGeometry(b.sx || 1, b.sz || 1, b.sy || 1);
    }

    // Координаты: симулятор → Three.js (y↔z swap)
    const p = agent.pose;
    const mesh = new THREE.Mesh(geometry, new THREE.MeshBasicMaterial({
        color: 0xff4444,
        transparent: true,
        opacity: 0.25,
        depthWrite: false,
    }));
    mesh.position.set(p.x, p.z || 0, -(p.y || 0));
    scene.add(mesh);
    collisionMeshes[agentKey] = mesh;
}

// ============================================================
// Обновление сцены из JSON
// ============================================================

function updateScene(data) {
    // Скрыть overlay загрузки сцены при получении первого снапшота
    hideLoadingOverlay();
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
                // Точки лидара: ищем плагин с type == "lidar_points"
                for (const [pluginType, pluginData] of Object.entries(agentPlugins)) {
                    const d = typeof pluginData === 'string'
                        ? JSON.parse(pluginData)
                        : pluginData;
                    if (d && d.type === 'lidar_points') {
                        renderLidarPoints(`lidar_${agentKey}_${pluginType}`, d);
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

    // Статическая геометрия — обновляем всякий раз, когда сервер присылает geometry.
    // Сервер присылает geometry при первом подключении и после POST /api/scene/geometry.
    // В режиме редактора не трогаем меши (редактор управляет ими сам).
    if (data.geometry) {
        staticGeometryData = data.geometry;  // кешируем для входа в editor mode
        if (!editorMode) {
            Object.keys(meshes).forEach(k => { if (k.startsWith('static_')) removeMesh(k); });
            data.geometry.forEach((geom, i) => {
                const pose = {
                    x:     geom.x     || 0,
                    y:     geom.y     || 0,
                    z:     geom.z     || 0,
                    yaw:   geom.yaw   || 0,
                    pitch: geom.pitch || 0,
                    roll:  geom.roll  || 0,
                };
                const visual = {
                    type:   geom.type   || 'box',
                    size:   [geom.sx || 1, geom.sy || 1, geom.sz || 1],
                    color:  geom.color  || '#808080',
                    radius: geom.radius || 0.5,
                    height: geom.height || 1.0,
                };
                updateOrCreateMesh(`static_${i}`, geom.type, pose, visual, { doubleSide: true });
            });
        }
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

            // Обновляем полупрозрачный коллизионный шейп (если кнопка включена)
            updateCollisionMesh(key, agent);
        });
    }
    Object.keys(meshes).forEach(k => {
        // agent_edit_* — превью редактора, живут отдельно от SSE-снапшота
        if (k.startsWith('agent_') && !k.startsWith('agent_edit_') && !currentAgentKeys.has(k)) removeMesh(k);
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

// Кнопка "Collisions": показать/скрыть полупрозрачные шейпы коллизий агентов
function toggleCollisions() {
    collisionsVisible = !collisionsVisible;
    const btn = document.getElementById('btn-collisions');
    if (btn) btn.textContent = collisionsVisible ? 'Collisions: ON' : 'Collisions: OFF';

    if (!collisionsVisible) {
        // Убираем все шейпы
        for (const key of Object.keys(collisionMeshes)) {
            scene.remove(collisionMeshes[key]);
            collisionMeshes[key].geometry.dispose();
            collisionMeshes[key].material.dispose();
            delete collisionMeshes[key];
        }
    }
    // При включении — шейпы появятся на следующем снапшоте через updateCollisionMesh()
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

    // Режим размещения агента — кликаем в плоскость земли
    if (placingAgentMode) {
        const pos = raycastOnGroundPlane(event);
        if (pos) {
            // Конвертируем: Three.js X→sim X, Three.js Z→sim -Y
            const simX = pos.x;
            const simY = -pos.z;
            cancelPlaceAgent();
            openAgentForm(null, simX, simY);
        }
        return;
    }

    // Shift+ЛКМ в режиме редактора — edge-snapping
    if (editorMode && shiftHeld) {
        handleEdgeSnapClick(event);
        return;
    }

    // В режиме редактора — кликаем по примитивам
    if (editorMode) {
        const staticMeshList = Object.entries(meshes)
            .filter(([k]) => k.startsWith('static_'))
            .map(([, m]) => m);
        const staticHits = raycaster.intersectObjects(staticMeshList);
        if (staticHits.length > 0) {
            const key = staticHits[0].object.userData.key;   // "static_prim_N"
            const id = key.replace('static_', '');
            selectPrimitive(id);
        } else {
            // Клик в пустоту — снять выбор
            transformControls.detach();
            selectedPrimitiveId = null;
            document.getElementById('primitive-props').style.display = 'none';
        }
        return;
    }

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

// Отслеживание Shift для edge-snapping + горячие клавиши редактора
window.addEventListener('keydown', e => {
    if (e.key === 'Shift') shiftHeld = true;

    if (!editorMode) return;

    if ((e.ctrlKey || e.metaKey) && !e.shiftKey) {
        switch (e.key.toLowerCase()) {
            case 'z': e.preventDefault(); undo(); break;
            case 'c': e.preventDefault(); copySelected(); break;
            case 'v': e.preventDefault(); pasteSelected(); break;
        }
    }

    if (e.key === 'Delete' || e.key === 'Backspace') {
        if (document.activeElement === document.body ||
            document.activeElement === renderer.domElement) {
            e.preventDefault();
            deleteSelected();
        }
    }
});
window.addEventListener('keyup', e => {
    if (e.key === 'Shift') {
        shiftHeld = false;
        // Сбросить незавершённый snap при отпускании Shift
        if (snapEdge1 && !snapEdge2) clearEdgeSnap();
    }
});

// Превью агента при размещении: полупрозрачный бокс следует за курсором
renderer.domElement.addEventListener('mousemove', (event) => {
    if (manualPanning) {
        updateManualPan(event);
        return;
    }
    if (!placingAgentMode) return;
    const pos = raycastOnGroundPlane(event);
    if (!pos) return;
    const sz = 0.3;
    const pose = { x: pos.x, y: -pos.z, z: sz / 2, yaw: 0, pitch: 0, roll: 0 };
    const visual = { type: 'box', color: '#FF6B35', size: [0.6, 0.4, sz] };
    updateOrCreateMesh('agent_place_preview', 'box', pose, visual,
        { transparent: true, opacity: 0.4 });
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
        // При реконнекте очищаем старые static меши — придут свежие с geometry в первом снапшоте
        if (!editorMode) {
            Object.keys(meshes).forEach(k => { if (k.startsWith('static_')) removeMesh(k); });
        }
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

// ============================================================
// Редактор агентов — загрузка данных с сервера
// ============================================================

/** Загрузить и кешировать реестр плагинов */
async function fetchPluginRegistry() {
    if (pluginRegistry.length > 0) return pluginRegistry;
    try {
        const host = window.location.hostname || 'localhost';
        const port = window.location.port || '1937';
        const r = await fetch(`http://${host}:${port}/api/plugins/registry`);
        pluginRegistry = await r.json();
    } catch (e) {
        console.error('[PluginRegistry] ошибка загрузки:', e);
        pluginRegistry = [];
    }
    return pluginRegistry;
}

/** Загрузить текущее состояние сцены (агенты) */
async function fetchSceneAgents() {
    try {
        const host = window.location.hostname || 'localhost';
        const port = window.location.port || '1937';
        const r = await fetch(`http://${host}:${port}/api/scene/state`);
        const data = await r.json();
        if (data.agents) {
            editorAgents = data.agents.map((a, i) => ({
                localId: nextAgentLocalId++,
                name: a.name || `robot_${i}`,
                domain_id: a.domain_id || 0,
                pose: {
                    x: a.pose?.x || 0,
                    y: a.pose?.y || 0,
                    z: a.pose?.z || 0,
                    yaw: a.pose?.yaw || 0
                },
                visual: {
                    type: a.visual?.type || 'box',
                    size: a.visual?.size || [0.6, 0.4, 0.3],
                    color: a.visual?.color || '#FF6B35',
                },
                urdf: a.urdf || '',
                plugins: (a.plugins || []).map(p => ({ ...p })),
            }));
            // Показываем preview-меши для существующих агентов
            editorAgents.forEach(a => createAgentPreviewMesh(a));
        }
    } catch (e) {
        console.error('[SceneState] ошибка загрузки:', e);
    }
}

/** Загрузить список URDF-файлов и заполнить select */
async function fetchUrdfList() {
    if (urdfList.length > 0) return;
    try {
        const host = window.location.hostname || 'localhost';
        const port = window.location.port || '1937';
        const r = await fetch(`http://${host}:${port}/api/scene/urdf-list`);
        const data = await r.json();
        urdfList = data.files || [];
    } catch (e) {
        urdfList = [];
    }
    // Заполняем select если форма открыта
    const sel = document.getElementById('af-urdf');
    if (sel) {
        while (sel.options.length > 1) sel.remove(1);
        urdfList.forEach(f => {
            const opt = document.createElement('option');
            opt.value = f;
            opt.textContent = f.split('/').pop();
            sel.appendChild(opt);
        });
    }
}

// ============================================================
// Редактор агентов — управление вкладками
// ============================================================

window.switchEditorTab = function(tab) {
    activeEditorTab = tab;
    document.getElementById('editor-tab-geometry').style.display =
        tab === 'geometry' ? '' : 'none';
    document.getElementById('editor-tab-agents').style.display =
        tab === 'agents' ? '' : 'none';
    document.querySelectorAll('.editor-tab-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.tab === tab);
    });

    if (tab === 'agents') {
        // Загружаем реестр и агентов при первом открытии
        fetchPluginRegistry().then(() => {
            if (editorAgents.length === 0) {
                fetchSceneAgents().then(() => renderAgentList());
            } else {
                renderAgentList();
            }
        });
        fetchUrdfList();
    } else {
        closeAgentForm();
    }
};

// ============================================================
// Редактор агентов — preview-меши
// ============================================================

function createAgentPreviewMesh(agent) {
    const key = `agent_edit_${agent.localId}`;
    const size = agent.visual.size || [0.6, 0.4, 0.3];
    const color = agent.visual.color || '#FF6B35';
    removeMesh(key);
    const pose = {
        x: agent.pose.x, y: agent.pose.y, z: (agent.pose.z || 0) + (size[2] || 0.3) / 2,
        yaw: agent.pose.yaw || 0, pitch: 0, roll: 0
    };
    const visual = { type: 'box', size: [size[0] || 0.6, size[1] || 0.4, size[2] || 0.3], color };
    updateOrCreateMesh(key, 'box', pose, visual);
    const m = meshes[key];
    if (m) {
        m.userData.key = key;
        m.userData.agentLocalId = agent.localId;
    }
}

function removeAgentPreviewMesh(localId) {
    removeMesh(`agent_edit_${localId}`);
}

// ============================================================
// Редактор агентов — список
// ============================================================

function renderAgentList() {
    const list = document.getElementById('agent-list');
    if (!list) return;
    list.innerHTML = '';

    if (editorAgents.length === 0) {
        list.innerHTML = '<div style="color:#666;font-size:11px;">Агентов нет</div>';
        return;
    }

    editorAgents.forEach(agent => {
        const pluginNames = (agent.plugins || []).map(p => p.type).join(' + ') || '—';

        const row = document.createElement('div');
        row.className = 'agent-list-item';

        const nameSpan = document.createElement('span');
        nameSpan.className = 'agent-name';
        nameSpan.textContent = `[${agent.name}]`;

        const pluginsSpan = document.createElement('span');
        pluginsSpan.className = 'agent-plugins';
        pluginsSpan.textContent = pluginNames;

        const editBtn = document.createElement('button');
        editBtn.textContent = '✎';
        editBtn.title = 'Редактировать';
        editBtn.onclick = () => openAgentForm(agent);

        const delBtn = document.createElement('button');
        delBtn.textContent = '✕';
        delBtn.className = 'danger';
        delBtn.title = 'Удалить';
        delBtn.onclick = () => deleteAgent(agent.localId);

        row.appendChild(nameSpan);
        row.appendChild(pluginsSpan);
        row.appendChild(editBtn);
        row.appendChild(delBtn);
        list.appendChild(row);
    });
}

// ============================================================
// Редактор агентов — размещение
// ============================================================

window.startPlaceAgent = function() {
    placingAgentMode = true;
    renderer.domElement.style.cursor = 'crosshair';
    showToast('Кликните в сцену для размещения агента');
};

function cancelPlaceAgent() {
    placingAgentMode = false;
    renderer.domElement.style.cursor = '';
    removeMesh('agent_place_preview');
}

/** Перерисовать превью нового агента по текущим значениям формы */
function refreshNewAgentPreview() {
    if (editingAgentLocalId !== null) return;
    const x     = parseFloat(document.getElementById('af-x')?.value) || 0;
    const y     = parseFloat(document.getElementById('af-y')?.value) || 0;
    const yaw   = parseFloat(document.getElementById('af-yaw')?.value) || 0;
    const color = document.getElementById('af-color')?.value || '#FF6B35';
    const sx    = parseFloat(document.getElementById('af-sx')?.value) || 0.6;
    const sy    = parseFloat(document.getElementById('af-sy')?.value) || 0.4;
    const sz    = parseFloat(document.getElementById('af-sz')?.value) || 0.3;
    removeMesh('agent_edit_pending');
    const pose   = { x, y, z: sz / 2, yaw, pitch: 0, roll: 0 };
    const visual = { type: 'box', size: [sx, sy, sz], color };
    updateOrCreateMesh('agent_edit_pending', 'box', pose, visual,
        { transparent: true, opacity: 0.5 });
}

/** Raycast на плоскость Y=0 (= Z=0 симулятора) */
function raycastOnGroundPlane(event) {
    const rect = renderer.domElement.getBoundingClientRect();
    const mx = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    const my = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    raycaster.setFromCamera({ x: mx, y: my }, camera);
    const target = new THREE.Vector3();
    raycaster.ray.intersectPlane(groundPlane, target);
    return target;
}

// ============================================================
// Редактор агентов — форма
// ============================================================

/** Открыть форму создания/редактирования агента */
function openAgentForm(existingAgent, posX, posY) {
    document.getElementById('agents-list-view').style.display = 'none';
    document.getElementById('agent-form-view').style.display = '';

    editingAgentLocalId = existingAgent ? existingAgent.localId : null;
    document.getElementById('af-title').textContent =
        existingAgent ? `Редактировать: ${existingAgent.name}` : 'Новый агент';

    const name    = existingAgent?.name || `robot_${editorAgents.length}`;
    const domain  = existingAgent?.domain_id ?? 0;
    const x       = posX !== undefined ? posX : (existingAgent?.pose?.x ?? 0);
    const y       = posY !== undefined ? posY : (existingAgent?.pose?.y ?? 0);
    const yaw     = existingAgent?.pose?.yaw ?? 0;
    const color   = existingAgent?.visual?.color ?? '#FF6B35';
    const size    = existingAgent?.visual?.size ?? [0.6, 0.4, 0.3];
    const urdf    = existingAgent?.urdf ?? '';

    document.getElementById('af-name').value    = name;
    document.getElementById('af-domain').value  = domain;
    document.getElementById('af-x').value       = x.toFixed(2);
    document.getElementById('af-y').value       = y.toFixed(2);
    document.getElementById('af-yaw').value     = yaw.toFixed(3);
    document.getElementById('af-color').value   = color;
    document.getElementById('af-sx').value      = (size[0] ?? 0.6).toFixed(2);
    document.getElementById('af-sy').value      = (size[1] ?? 0.4).toFixed(2);
    document.getElementById('af-sz').value      = (size[2] ?? 0.3).toFixed(2);

    // Заполнить URDF select
    const sel = document.getElementById('af-urdf');
    while (sel.options.length > 1) sel.remove(1);
    urdfList.forEach(f => {
        const opt = document.createElement('option');
        opt.value = f;
        opt.textContent = f.split('/').pop();
        sel.appendChild(opt);
    });
    sel.value = urdf;

    // Построить форму плагинов
    buildPluginForm(existingAgent?.plugins || []);

    // Превью нового агента: полупрозрачный бокс с live-обновлением по полям формы
    if (!existingAgent) {
        const formEl = document.getElementById('agent-form-view');
        if (_newAgentFormListener) formEl.removeEventListener('input', _newAgentFormListener);
        _newAgentFormListener = refreshNewAgentPreview;
        formEl.addEventListener('input', _newAgentFormListener);
        refreshNewAgentPreview();
    }
}

window.closeAgentForm = function() {
    document.getElementById('agent-form-view').style.display = 'none';
    document.getElementById('agents-list-view').style.display = '';
    editingAgentLocalId = null;
    cancelPlaceAgent();
    // Убираем превью нового агента и listener
    removeMesh('agent_edit_pending');
    const formEl = document.getElementById('agent-form-view');
    if (_newAgentFormListener) {
        formEl.removeEventListener('input', _newAgentFormListener);
        _newAgentFormListener = null;
    }
};

/** Построить секцию плагинов в форме из pluginRegistry */
function buildPluginForm(activePlugins) {
    const container = document.getElementById('af-plugins-list');
    if (!container) return;
    container.innerHTML = '';

    for (const pluginDef of pluginRegistry) {
        const activePlugin = activePlugins.find(p => p.type === pluginDef.type);
        const isEnabled = !!activePlugin;

        const row = document.createElement('div');
        row.className = 'af-plugin-row';

        // Заголовок с чекбоксом
        const header = document.createElement('div');
        header.className = 'af-plugin-header';

        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.id = `af-cb-${pluginDef.type}`;
        cb.checked = isEnabled;

        const lbl = document.createElement('label');
        lbl.htmlFor = cb.id;
        lbl.textContent = pluginDef.label;

        header.appendChild(cb);
        header.appendChild(lbl);
        row.appendChild(header);

        // Параметры (показываем только если плагин включён и есть параметры)
        if (pluginDef.params && pluginDef.params.length > 0) {
            const paramsDiv = document.createElement('div');
            paramsDiv.className = 'af-plugin-params' + (isEnabled ? ' open' : '');
            paramsDiv.id = `af-params-${pluginDef.type}`;

            for (const param of pluginDef.params) {
                const currentVal = activePlugin?.[param.key] ?? param.default;
                const paramRow = document.createElement('div');
                paramRow.className = 'af-param-row';

                const paramLbl = document.createElement('span');
                paramLbl.textContent = param.label + ':';

                const input = document.createElement('input');
                input.id = `af-param-${pluginDef.type}-${param.key}`;
                if (param.type === 'color') {
                    input.type = 'color';
                    input.value = currentVal;
                } else if (param.type === 'number') {
                    input.type = 'number';
                    input.step = '0.01';
                    input.value = currentVal;
                } else {
                    input.type = 'text';
                    input.value = currentVal;
                }

                paramRow.appendChild(paramLbl);
                paramRow.appendChild(input);
                paramsDiv.appendChild(paramRow);
            }

            row.appendChild(paramsDiv);

            // Переключение видимости параметров при клике на чекбокс
            cb.addEventListener('change', () => {
                paramsDiv.classList.toggle('open', cb.checked);
            });
        }

        container.appendChild(row);
    }
}

/** Прочитать текущие значения из формы и вернуть объект агента */
function readAgentFromForm() {
    const name     = document.getElementById('af-name').value.trim() || 'robot_0';
    const domain   = parseInt(document.getElementById('af-domain').value) || 0;
    const x        = parseFloat(document.getElementById('af-x').value) || 0;
    const y        = parseFloat(document.getElementById('af-y').value) || 0;
    const yaw      = parseFloat(document.getElementById('af-yaw').value) || 0;
    const color    = document.getElementById('af-color').value;
    const sx       = parseFloat(document.getElementById('af-sx').value) || 0.6;
    const sy       = parseFloat(document.getElementById('af-sy').value) || 0.4;
    const sz       = parseFloat(document.getElementById('af-sz').value) || 0.3;
    const urdf     = document.getElementById('af-urdf').value;

    // Собираем плагины из формы
    const plugins = [];
    for (const pluginDef of pluginRegistry) {
        const cb = document.getElementById(`af-cb-${pluginDef.type}`);
        if (!cb || !cb.checked) continue;

        const plugin = { type: pluginDef.type };
        for (const param of (pluginDef.params || [])) {
            const input = document.getElementById(`af-param-${pluginDef.type}-${param.key}`);
            if (!input) continue;
            if (param.type === 'number') {
                plugin[param.key] = parseFloat(input.value);
            } else {
                plugin[param.key] = input.value;
            }
        }
        plugins.push(plugin);
    }

    return {
        name, domain_id: domain,
        pose: { x, y, z: 0, yaw },
        visual: { type: 'box', size: [sx, sy, sz], color },
        urdf,
        plugins,
    };
}

/** Подтвердить форму: добавить или обновить агента в editorAgents */
window.confirmAgent = function() {
    const agentData = readAgentFromForm();

    if (editingAgentLocalId !== null) {
        // Обновление существующего агента
        const idx = editorAgents.findIndex(a => a.localId === editingAgentLocalId);
        if (idx >= 0) {
            removeAgentPreviewMesh(editingAgentLocalId);
            editorAgents[idx] = { ...agentData, localId: editingAgentLocalId };
            createAgentPreviewMesh(editorAgents[idx]);
        }
    } else {
        // Новый агент: убираем временное превью, создаём постоянный меш
        removeMesh('agent_edit_pending');
        const localId = nextAgentLocalId++;
        const newAgent = { ...agentData, localId };
        editorAgents.push(newAgent);
        createAgentPreviewMesh(newAgent);
    }

    closeAgentForm();
    renderAgentList();
};

/** Удалить агента из редактора */
function deleteAgent(localId) {
    if (!confirm('Удалить агента?')) return;
    editorAgents = editorAgents.filter(a => a.localId !== localId);
    removeAgentPreviewMesh(localId);
    renderAgentList();
}

/** Отправить список агентов на сервер для сохранения в YAML */
async function sendAgentsToServer() {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';

    // Преобразуем editorAgents в формат для YAML
    const agents = editorAgents.map(a => {
        const obj = {
            name: a.name,
            domain_id: a.domain_id,
            pose: { x: a.pose.x, y: a.pose.y, z: a.pose.z || 0, yaw: a.pose.yaw || 0 },
            visual: {
                type: a.visual.type || 'box',
                size: a.visual.size || [0.6, 0.4, 0.3],
                color: a.visual.color || '#FF6B35',
            },
            plugins: a.plugins || [],
        };
        if (a.urdf) obj.urdf = a.urdf;
        return obj;
    });

    await fetch(`http://${host}:${port}/api/scene/agents`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(agents),
    });
}

// Экспортируем функции в глобальную область для onclick
window.sendCommand = sendCommand;
window.toggleFollow = toggleFollow;
window.closeSidePanel = closeSidePanel;
window.toggleTransformMode = toggleTransformMode;
window.toggleTransformControls = toggleTransformControls;
window.toggleCollisions = toggleCollisions;
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

// ============================================================
// Браузер сцен (задача 19)
// ============================================================

let scenesPanelOpen = false;

function showLoadingOverlay() {
    const el = document.getElementById('loading-overlay');
    if (el) el.style.display = 'flex';
}

function hideLoadingOverlay() {
    const el = document.getElementById('loading-overlay');
    if (el) el.style.display = 'none';
}

window.toggleScenesPanel = function() {
    scenesPanelOpen = !scenesPanelOpen;
    document.getElementById('scenes-panel').style.display =
        scenesPanelOpen ? 'block' : 'none';
    document.getElementById('btn-scenes').classList.toggle('active', scenesPanelOpen);
    if (scenesPanelOpen) loadSceneList();
};

function loadSceneList() {
    const listEl = document.getElementById('scenes-list');
    listEl.innerHTML = '<div style="color:#666;">Загрузка...</div>';
    fetch('/api/scenes')
        .then(r => r.json())
        .then(data => {
            if (!data.scenes || data.scenes.length === 0) {
                listEl.innerHTML = '<div style="color:#666;">Сцены не найдены</div>';
                return;
            }
            listEl.innerHTML = data.scenes.map(name =>
                `<div style="padding:5px 0; border-bottom:1px solid #333; display:flex; justify-content:space-between; align-items:center;">
                   <span style="overflow:hidden; text-overflow:ellipsis; white-space:nowrap; flex:1; margin-right:6px;">${name}</span>
                   <button onclick="loadScene('${name}')"
                     style="background:#2980b9; color:#fff; border:none; padding:2px 8px;
                            border-radius:3px; cursor:pointer; font-size:11px; flex-shrink:0;">
                     Load
                   </button>
                 </div>`
            ).join('');
        })
        .catch(e => {
            listEl.innerHTML = `<div style="color:#f44;">Ошибка: ${e.message}</div>`;
        });
}

window.loadScene = function(filename) {
    if (!confirm(`Загрузить сцену "${filename}"?\nСимуляция будет перезапущена.`)) return;
    showLoadingOverlay();
    fetch('/api/scene/load', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ filename }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) {
            resetEditorState();
            showToast(`Сцена "${filename}" загружена`);
            if (scenesPanelOpen) {
                scenesPanelOpen = false;
                document.getElementById('scenes-panel').style.display = 'none';
                document.getElementById('btn-scenes').classList.remove('active');
            }
        } else {
            hideLoadingOverlay();
            showToast(`Ошибка загрузки: ${d.error}`);
        }
    })
    .catch(e => {
        hideLoadingOverlay();
        showToast(`Ошибка: ${e.message}`);
    });
};

window.saveSceneAs = function() {
    const name = prompt('Имя новой копии сцены (без расширения):');
    if (!name) return;
    fetch('/api/scene/save-as', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) {
            showToast(`Сохранено: ${d.path}`);
            loadSceneList();
        } else {
            showToast(`Ошибка: ${d.error}`);
        }
    })
    .catch(e => showToast(`Ошибка: ${e.message}`));
};

window.newScene = function() {
    const name = prompt('Имя новой сцены:');
    if (!name) return;
    showLoadingOverlay();
    fetch('/api/scene/new', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) {
            resetEditorState();
            showToast(`Создана и загружена сцена "${d.name}"`);
            if (scenesPanelOpen) {
                scenesPanelOpen = false;
                document.getElementById('scenes-panel').style.display = 'none';
                document.getElementById('btn-scenes').classList.remove('active');
            }
        } else {
            hideLoadingOverlay();
            showToast(`Ошибка: ${d.error}`);
        }
    })
    .catch(e => {
        hideLoadingOverlay();
        showToast(`Ошибка: ${e.message}`);
    });
};

/**
 * Сбросить состояние редактора и Three.js сцены после загрузки новой сцены.
 */
function resetEditorState() {
    // Выйти из режима редактора если открыт
    if (editorMode) toggleEditorMode(false);
    // Снять выделение
    transformControls.detach();
    selectedPrimitiveId = null;
    selectedAgentId = null;
    selectedAgentMesh = null;
    // Очистить меши статической геометрии
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('static_')) removeMesh(k);
    });
    // Очистить меши агентов, пропов, акторов
    Object.keys(meshes).forEach(k => {
        if (k.startsWith('agent_') || k.startsWith('prop_') || k.startsWith('actor_'))
            removeMesh(k);
    });
    // Сбросить состояние редактора
    editorPrimitives = [];
    editorAgents = [];
    staticGeometryData = [];
    lastAgentData = {};
    followMode = false;
    followCameraOffset = null;
    // Скрыть боковую панель
    document.getElementById('primitive-props').style.display = 'none';
}