const LABELS = {
  fire: "Fire",
  rainbow: "Rainbow",
  metaballs: "Meatballs",
  plasma: "Plasma",
  sun: "Sun",
  spiral: "Spiral",
  fluid: "Fluid",
  heart: "Heart",
  matrix: "Matrix",
  lightning: "Lightning",
};

const state = { mode: 0, speed: 50, fader: 50, name: "fire" };
const note = document.getElementById("spdnote");
const err = document.getElementById("err");
const canvas = document.getElementById("panel");
const ctx = canvas.getContext("2d");
const N = 16;
const CELL = canvas.width / N;

function noteFor(name) {
  if (name === "spiral") note.textContent = "Speed shifts the spiral colour. Spin stays fixed.";
  else if (name === "matrix") note.textContent = "Speed is how fast the rain falls.";
  else note.textContent = "Speed is how fast this preset moves.";
}

function drawLed(rgb, ptr) {
  ctx.fillStyle = "#070604";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  const gap = CELL * 0.16;
  const r = (CELL - gap) / 2;
  for (let y = 0; y < N; y++) {
    for (let x = 0; x < N; x++) {
      const i = (y * N + x) * 3;
      const R = rgb[ptr + i];
      const G = rgb[ptr + i + 1];
      const B = rgb[ptr + i + 2];
      ctx.beginPath();
      ctx.fillStyle = "rgb(" + R + "," + G + "," + B + ")";
      ctx.arc(x * CELL + CELL / 2, y * CELL + CELL / 2, r, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

function ballRow(api) {
  const box = document.getElementById("balls");
  const n = api.balls();
  if (state.name === "metaballs") box.removeAttribute("hidden");
  else box.setAttribute("hidden", "");
  document.getElementById("bcnt").textContent = n + " of 8";
  document.getElementById("badd").disabled = n >= 8;
  document.getElementById("bmin").disabled = n <= 1;
}

async function boot() {
  if (typeof LampModule !== "function") {
    err.textContent = "lamp.js is missing. From web/simulator run: npm run build";
    return;
  }
  const mod = await LampModule();
  const api = {
    init: mod.cwrap("lamp_init", null, []),
    set: mod.cwrap("lamp_set", null, ["number", "number", "number"]),
    dirty: mod.cwrap("lamp_mark_dirty", null, []),
    count: mod.cwrap("lamp_mode_count", "number", []),
    name: mod.cwrap("lamp_mode_name", "string", ["number"]),
    add: mod.cwrap("lamp_balls_add", null, []),
    remove: mod.cwrap("lamp_balls_remove", null, []),
    balls: mod.cwrap("lamp_balls", "number", []),
    step: mod.cwrap("lamp_step", "number", ["number"]),
    frame: mod.cwrap("lamp_frame", "number", ["number"]),
  };
  api.init();
  const names = [];
  const grid = document.getElementById("grid");
  for (let i = 0; i < api.count(); i++) {
    const name = api.name(i);
    names.push(name);
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = LABELS[name] || name;
    b.dataset.i = String(i);
    b.onclick = () => {
      state.mode = i;
      state.name = name;
      api.set(state.mode, state.speed, state.fader);
      api.dirty();
      paintButtons();
      noteFor(name);
      ballRow(api);
    };
    grid.appendChild(b);
  }
  function paintButtons() {
    grid.querySelectorAll("button").forEach((b) => {
      b.className = Number(b.dataset.i) === state.mode ? "on" : "";
    });
  }
  document.getElementById("bri").oninput = (e) => {
    state.fader = Number(e.target.value);
    api.set(state.mode, state.speed, state.fader);
  };
  document.getElementById("spd").oninput = (e) => {
    state.speed = Number(e.target.value);
    api.set(state.mode, state.speed, state.fader);
    noteFor(state.name);
  };
  document.getElementById("badd").onclick = () => {
    api.add();
    ballRow(api);
  };
  document.getElementById("bmin").onclick = () => {
    api.remove();
    ballRow(api);
  };
  paintButtons();
  noteFor(state.name);
  ballRow(api);
  api.set(state.mode, state.speed, state.fader);

  let last = performance.now();
  function tick(now) {
    let dt = now - last;
    last = now;
    if (dt < 0) dt = 0;
    if (dt > 100) dt = 100;
    api.step(Math.round(dt));
    const ptr = api.frame(state.fader);
    const rgb = mod.HEAPU8;
    drawLed(rgb, ptr);
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}

const script = document.createElement("script");
script.src = "lamp.js";
script.onload = () => boot().catch((e) => {
  err.textContent = String(e && e.message ? e.message : e);
});
script.onerror = () => {
  err.textContent = "Could not load lamp.js. From web/simulator run: npm run build";
};
document.head.appendChild(script);
