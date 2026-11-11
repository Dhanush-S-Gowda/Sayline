"use strict";

(() => {
  const demo = document.querySelector("#demo");
  const handle = document.querySelector("#drag-handle");
  const transcript = document.querySelector("#transcript");
  const toggle = document.querySelector("#demo-toggle");
  const toggleLabel = document.querySelector("#toggle-label");
  const hint = document.querySelector("#demo-hint");
  const announcement = document.querySelector("#announcement");
  const punctuation = document.querySelector("#punctuation");
  const replay = document.querySelector("#replay");
  const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const samples = [
    "Hello, I’m Sayline. A little space for your voice, wherever you’re working.",
    "A thought, a message, a note to yourself. Say it, and keep going.",
    "Give your keyboard a breather. There’s another way to get your words down.",
    "This is what a thought looks like when your keyboard takes the afternoon off."
  ];
  let previousSample = -1;
  let words = [];
  let wordCount = 0;
  let timer;
  let position = null;
  let drag = null;
  let focusBeforeOpen = null;

  function announce(message) {
    announcement.textContent = message;
  }

  function viewport() {
    return { width: document.documentElement.clientWidth, height: window.innerHeight };
  }

  function moveTo(x, y) {
    const { width, height } = viewport();
    position = {
      x: Math.max(16, Math.min(x, width - demo.offsetWidth - 16)),
      y: Math.max(16, Math.min(y, height - demo.offsetHeight - 16))
    };
    demo.style.transform = `translate(${position.x}px, ${position.y}px)`;
  }

  function renderTranscript() {
    const text = words.slice(0, wordCount).join(" ");
    transcript.textContent = punctuation.getAttribute("aria-pressed") === "true"
      ? text : text.replace(/[,.!?;:]/g, "");
    transcript.scrollLeft = transcript.scrollWidth;
  }

  function finishSample() {
    clearTimeout(timer);
    wordCount = words.length;
    renderTranscript();
    demo.classList.remove("is-playing");
    announce(`Sample transcript: ${transcript.textContent}. Drag the bar or use arrow keys to move it. Escape closes the demo.`);
  }

  function typeNextWord() {
    wordCount += 1;
    renderTranscript();
    if (wordCount >= words.length) {
      finishSample();
    } else {
      timer = setTimeout(typeNextWord, 170 + Math.random() * 130);
    }
  }

  function playSample() {
    clearTimeout(timer);
    // Pick a fresh sample on each replay without repeating the last one.
    const next = previousSample < 0
      ? Math.floor(Math.random() * samples.length)
      : (previousSample + 1 + Math.floor(Math.random() * (samples.length - 1))) % samples.length;
    previousSample = next;
    words = samples[next].split(" ");
    wordCount = 0;
    transcript.textContent = "Ready when you are…";
    announce("Playing a simulated dictation sample. No microphone is used.");
    if (reducedMotion.matches) {
      finishSample();
      return;
    }
    demo.classList.add("is-playing");
    timer = setTimeout(typeNextWord, 450);
  }

  function endDrag() {
    if (!drag) return;
    const { id } = drag;
    drag = null;
    demo.classList.remove("is-dragging");
    if (handle.hasPointerCapture(id)) handle.releasePointerCapture(id);
  }

  function closeDemo() {
    const shouldRestoreFocus = demo.contains(document.activeElement);
    clearTimeout(timer);
    endDrag();
    demo.hidden = true;
    demo.classList.remove("is-playing");
    toggle.setAttribute("aria-expanded", "false");
    toggleLabel.textContent = "Try the demo";
    hint.textContent = "Click above or press the shortcut.";
    announce("Demo closed.");
    if (shouldRestoreFocus) {
      const target = focusBeforeOpen?.isConnected && focusBeforeOpen !== document.body ? focusBeforeOpen : toggle;
      target.focus({ preventScroll: true });
    }
  }

  function openDemo() {
    focusBeforeOpen = document.activeElement;
    demo.hidden = false;
    const { width, height } = viewport();
    if (position) {
      moveTo(position.x, position.y);
    } else {
      moveTo(width > 1000 ? width * 0.56 : (width - demo.offsetWidth) / 2,
        width > 1000 ? height * 0.57 : height - demo.offsetHeight - 76);
    }
    toggle.setAttribute("aria-expanded", "true");
    toggleLabel.textContent = "Close demo";
    hint.textContent = "Drag the bar anywhere. Press Esc to close.";
    handle.focus({ preventScroll: true });
    playSample();
  }

  function toggleDemo() {
    if (demo.hidden) openDemo();
    else closeDemo();
  }

  toggle.addEventListener("click", toggleDemo);
  toggle.disabled = false;
  replay.addEventListener("click", playSample);
  punctuation.addEventListener("click", () => {
    const enabled = punctuation.getAttribute("aria-pressed") !== "true";
    punctuation.setAttribute("aria-pressed", String(enabled));
    if (wordCount > 0) renderTranscript();
    announce(`Spoken punctuation ${enabled ? "on" : "off"} in the sample.`);
  });

  document.addEventListener("keydown", (event) => {
    if (event.isComposing) return;
    const isShortcut = event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey && event.code === "Space";
    if (isShortcut) {
      event.preventDefault();
      if (!event.repeat) toggleDemo();
    } else if (event.key === "Escape" && !demo.hidden) {
      event.preventDefault();
      closeDemo();
    }
  });

  handle.addEventListener("keydown", (event) => {
    if (event.target !== handle || event.ctrlKey || event.metaKey || event.altKey) return;
    const directions = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
    const direction = directions[event.key];
    if (!direction) return;
    event.preventDefault();
    const step = event.shiftKey ? 32 : 12;
    moveTo(position.x + direction[0] * step, position.y + direction[1] * step);
  });

  handle.addEventListener("pointerdown", (event) => {
    if (!event.isPrimary || event.button !== 0 || event.target.closest("button")) return;
    drag = { id: event.pointerId, x: event.clientX - position.x, y: event.clientY - position.y };
    handle.setPointerCapture(event.pointerId);
    handle.focus({ preventScroll: true });
    demo.classList.add("is-dragging");
  });
  handle.addEventListener("pointermove", (event) => {
    if (drag?.id !== event.pointerId) return;
    moveTo(event.clientX - drag.x, event.clientY - drag.y);
  });
  handle.addEventListener("pointerup", endDrag);
  handle.addEventListener("pointercancel", endDrag);
  handle.addEventListener("lostpointercapture", endDrag);
  window.addEventListener("blur", endDrag);
  window.addEventListener("resize", () => {
    if (!demo.hidden && position) {
      endDrag();
      moveTo(position.x, position.y);
    }
  });
  document.addEventListener("visibilitychange", () => {
    if (document.hidden && !demo.hidden) finishSample();
  });
  reducedMotion.addEventListener("change", () => {
    if (reducedMotion.matches && !demo.hidden) finishSample();
  });
})();
