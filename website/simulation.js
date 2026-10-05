/* ========================================================
   Distributed Compute Engine — Interactive Simulation
   ======================================================== */

(function () {
  "use strict";

  // ── State ──────────────────────────────────────────────
  const SAMPLE_JOBS = [
    { name: "distributed_sum.py", output: "Input range: 1..10000\nComputed sum: 50005000" },
    { name: "matrix_multiply.py", output: "Matrix A (3×3) × B (3×3)\nResult: [[30,36,42],[66,81,96],[102,126,150]]" },
    { name: "word_count.py",      output: "Total words: 8,412\nUnique words: 1,203" },
    { name: "prime_sieve.py",     output: "Primes under 100,000: 9,592\nLargest: 99,991" },
    { name: "pi_estimate.py",     output: "Monte Carlo π ≈ 3.14152\nIterations: 1,000,000" },
    { name: "sort_benchmark.py",  output: "Sorted 500,000 integers in 247 ms\nAlgorithm: Timsort" },
  ];

  let workers = {
    worker1: { status: "idle", port: 9101, job: null },
    worker2: { status: "idle", port: 9102, job: null },
    worker3: { status: "idle", port: 9103, job: null },
  };

  let jobQueue     = [];
  let completedJobs = [];
  let jobCounter   = 0;
  let heartbeatInterval = null;
  let schedulerInterval = null;

  // ── DOM refs ───────────────────────────────────────────
  const submitBtn  = document.getElementById("sim-submit-btn");
  const killBtn    = document.getElementById("sim-kill-btn");
  const reviveBtn  = document.getElementById("sim-revive-btn");
  const resetBtn   = document.getElementById("sim-reset-btn");
  const logEl      = document.getElementById("sim-log");
  const queueEl    = document.getElementById("sim-queue-items");
  const canvasEl   = document.getElementById("sim-canvas");

  // ── Logging ────────────────────────────────────────────
  function log(text, cls = "log-system") {
    const ts = new Date().toLocaleTimeString("en-US", { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
    const entry = document.createElement("div");
    entry.className = `log-entry ${cls}`;
    entry.textContent = `${ts}  ${text}`;
    logEl.appendChild(entry);
    logEl.scrollTop = logEl.scrollHeight;
  }

  // ── Worker UI update ───────────────────────────────────
  function updateWorkerUI(wid) {
    const w = workers[wid];
    const el = document.getElementById(`sim-${wid}`);
    el.setAttribute("data-state", w.status);
    const statusEl = el.querySelector(".sim-node-status");
    if (w.status === "busy" && w.job) {
      statusEl.textContent = `busy · ${w.job}`;
    } else {
      statusEl.textContent = w.status;
    }
  }

  // ── Queue UI ───────────────────────────────────────────
  function renderQueue() {
    queueEl.innerHTML = "";
    jobQueue.forEach((j) => {
      const chip = document.createElement("span");
      chip.className = "sim-queue-item";
      chip.textContent = j.id;
      queueEl.appendChild(chip);
    });
  }

  // ── Particle animation ────────────────────────────────
  function spawnParticle(fromId, toId, cls, durationMs = 600) {
    const fromEl = document.getElementById(fromId);
    const toEl   = document.getElementById(toId);
    if (!fromEl || !toEl) return;

    const canvasRect = canvasEl.getBoundingClientRect();
    const fromRect   = fromEl.getBoundingClientRect();
    const toRect     = toEl.getBoundingClientRect();

    const sx = fromRect.left + fromRect.width / 2 - canvasRect.left;
    const sy = fromRect.top  + fromRect.height / 2 - canvasRect.top;
    const ex = toRect.left   + toRect.width / 2  - canvasRect.left;
    const ey = toRect.top    + toRect.height / 2  - canvasRect.top;

    const dot = document.createElement("div");
    dot.className = `sim-particle ${cls}`;
    dot.style.left = sx + "px";
    dot.style.top  = sy + "px";
    canvasEl.appendChild(dot);

    const anim = dot.animate(
      [
        { left: sx + "px", top: sy + "px", opacity: 1 },
        { left: ex + "px", top: ey + "px", opacity: 0.7 },
      ],
      { duration: durationMs, easing: "cubic-bezier(0.16,1,0.3,1)", fill: "forwards" }
    );

    anim.onfinish = () => dot.remove();
  }

  // ── Assign job to first idle worker ────────────────────
  function tryAssign(job) {
    for (const wid of Object.keys(workers)) {
      const w = workers[wid];
      if (w.status === "idle") {
        w.status = "busy";
        w.job = job.id;
        updateWorkerUI(wid);
        log(`[SCHEDULER] Assigned ${job.id} → ${wid}`, "log-scheduler");
        spawnParticle("sim-master", `sim-${wid}`, "sim-particle-run", 500);

        // Simulate job execution (2-4s)
        const execTime = 2000 + Math.random() * 2000;
        setTimeout(() => finishJob(wid, job), execTime);
        return true;
      }
    }
    return false;
  }

  // ── Job completion ─────────────────────────────────────
  function finishJob(wid, job) {
    const w = workers[wid];
    // Worker might have been killed during execution
    if (w.status === "dead") return;

    w.status = "idle";
    w.job = null;
    updateWorkerUI(wid);

    completedJobs.push(job);
    const elapsed = (2000 + Math.random() * 1500).toFixed(0);

    spawnParticle(`sim-${wid}`, "sim-master", "sim-particle-done", 500);

    log(`[RESULT] ${wid} completed ${job.id}`, "log-result");
    log(`[RESULT] Output: ${job.output}`, "log-result");
    log(`[FINAL] ${job.id} finished in ${elapsed} ms`, "log-result");

    // Check if any queued jobs can be assigned now
    drainQueue();
  }

  // ── Drain queue ────────────────────────────────────────
  function drainQueue() {
    const newQueue = [];
    for (const job of jobQueue) {
      if (!tryAssign(job)) {
        newQueue.push(job);
      }
    }
    jobQueue = newQueue;
    renderQueue();
  }

  // ── Submit ─────────────────────────────────────────────
  function submitJob() {
    jobCounter++;
    const sample = SAMPLE_JOBS[(jobCounter - 1) % SAMPLE_JOBS.length];
    const job = {
      id: `job${jobCounter}`,
      name: sample.name,
      output: sample.output,
    };

    log(`[MASTER] Job ${job.id} received — ${job.name}`, "log-master");
    spawnParticle("sim-master", "sim-master", "sim-particle-submit", 300);

    const assigned = tryAssign(job);
    if (!assigned) {
      jobQueue.push(job);
      renderQueue();
      log(`[MASTER] ${job.id} queued (all workers busy)`, "log-master");
    }
  }

  // ── Kill a random alive worker ─────────────────────────
  function killWorker() {
    const alive = Object.keys(workers).filter((w) => workers[w].status !== "dead");
    if (alive.length === 0) {
      log("[SYSTEM] No alive workers to kill.", "log-error");
      return;
    }
    const wid = alive[Math.floor(Math.random() * alive.length)];
    const w = workers[wid];
    const hadJob = w.job;

    w.status = "dead";
    const lostJob = w.job;
    w.job = null;
    updateWorkerUI(wid);

    log(`[MASTER] ${wid} missed heartbeat — marking dead`, "log-error");

    // Requeue running job
    if (lostJob) {
      const sample = SAMPLE_JOBS[(parseInt(lostJob.replace("job", "")) - 1) % SAMPLE_JOBS.length];
      const requeuedJob = { id: lostJob, name: sample.name, output: sample.output };
      jobQueue.push(requeuedJob);
      renderQueue();
      log(`[SCHEDULER] Requeueing ${lostJob}`, "log-scheduler");

      // Try to reassign immediately
      setTimeout(drainQueue, 500);
    }
  }

  // ── Revive all dead workers ────────────────────────────
  function reviveWorkers() {
    let revived = false;
    for (const wid of Object.keys(workers)) {
      if (workers[wid].status === "dead") {
        workers[wid].status = "idle";
        workers[wid].job = null;
        updateWorkerUI(wid);
        log(`[MASTER] ${wid} reconnected`, "log-heartbeat");
        revived = true;
      }
    }
    if (!revived) {
      log("[SYSTEM] All workers already alive.", "log-system");
      return;
    }
    // Try to assign queued jobs
    setTimeout(drainQueue, 400);
  }

  // ── Reset ──────────────────────────────────────────────
  function resetSim() {
    jobCounter = 0;
    jobQueue = [];
    completedJobs = [];

    for (const wid of Object.keys(workers)) {
      workers[wid].status = "idle";
      workers[wid].job = null;
      updateWorkerUI(wid);
    }

    renderQueue();
    logEl.innerHTML = '<div class="log-entry log-system">[SYSTEM] Simulation reset. Submit a job to begin.</div>';
  }

  // ── Heartbeat simulation ───────────────────────────────
  function heartbeatTick() {
    for (const wid of Object.keys(workers)) {
      if (workers[wid].status !== "dead") {
        spawnParticle(`sim-${wid}`, "sim-master", "sim-particle-heartbeat", 800);
      }
    }
  }

  // ── Scheduler loop (drains queue) ──────────────────────
  function schedulerTick() {
    if (jobQueue.length > 0) {
      drainQueue();
    }
  }

  // ── Init ───────────────────────────────────────────────
  function init() {
    // Set initial UI state
    for (const wid of Object.keys(workers)) {
      updateWorkerUI(wid);
    }

    submitBtn.addEventListener("click", submitJob);
    killBtn.addEventListener("click", killWorker);
    reviveBtn.addEventListener("click", reviveWorkers);
    resetBtn.addEventListener("click", resetSim);

    // Start heartbeat visuals every 5 seconds
    heartbeatInterval = setInterval(heartbeatTick, 5000);
    // Start scheduler check every 2 seconds
    schedulerInterval = setInterval(schedulerTick, 2000);
  }

  // Wait for DOM
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
