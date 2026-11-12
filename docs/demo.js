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
  // Each step pairs simulated recognized speech with its formatted output.
  const samples = [
    [["Hello", "Hello"], [" comma", ","], [" world", " world"], [" full stop", "."],
      [" next paragraph", "\n\n"], [" zero point two", "0.2"]],
    [["start key point", "- "], [" First idea", "First idea"],
      [" next point", "\n- "], [" Second idea", "Second idea"],
      [" stop key point", "\n"], [" Back to ordinary text", "Back to ordinary text"]],
    [["caps lock on", ""], ["hello", "HELLO"], [" world", " WORLD"],
      [" caps lock off", ""], [" normal casing", " normal casing"], [" full stop", "."]],
    [["Hello", "Hello"], [" space", " "], [" worldx", "worldx"],
      [" backspace", "\b"], [" exclamation mark", "!"],
      [" new line", "\n"], [" literal comma", "comma"]]
  ];
  const help = document.querySelector("#command-help");
  const guide = document.querySelector("#command-guide");
  const closeGuideButton = document.querySelector("#close-guide");
  const guideTables = document.querySelector("#guide-tables");
  guideTables.append(
    ...Array.from(document.querySelector("#command-tables").children, table => table.cloneNode(true))
  );
  const compactLabels = new Map([
    ["open quote / close quote", "open / close quote"],
    ["open parenthesis / close parenthesis", "open / close parenthesis"],
    ["open bracket / close bracket", "open / close bracket"],
    ["Delete one character from this dictation", "Delete 1 character"],
    ["new paragraph / next paragraph", "new / next paragraph"],
    ["point + single digit or digit word", "point + digit"],
    ["Decimal: zero point two → 0.2", "Decimal: 0.2"],
    ["UPPERCASE dictated text", "UPPERCASE"],
    ["caps lock off / caps lock of", "caps lock off / of"],
    ["Return to normal casing", "Normal casing"],
    ["Next - item, only while list mode is on", "Next - item (list on)"],
    ["End list; start a plain new line", "End list; new line"],
    ["Keep the command words: literal comma → comma", "Keep command words"]
  ]);
  for (const cell of guideTables.querySelectorAll("td")) {
    if (compactLabels.has(cell.textContent)) cell.textContent = compactLabels.get(cell.textContent);
  }
  let showGuideTimer;
  let hideGuideTimer;

  function positionGuide() {
    const rect = demo.getBoundingClientRect();
    const above = Math.max(0, rect.top - 12);
    const below = Math.max(0, window.innerHeight - rect.bottom - 12);
    const useAbove = above >= below;
    guide.style.maxHeight = `${Math.min(560, Math.max(above, below) - 8)}px`;
    guide.style.left = `${Math.max(12, Math.min(rect.right - guide.offsetWidth, viewport().width - guide.offsetWidth - 12))}px`;
    guide.style.top = `${useAbove ? Math.max(12, rect.top - guide.offsetHeight - 8) : rect.bottom + 8}px`;
  }

  function closeGuide(restoreFocus = false) {
    clearTimeout(showGuideTimer);
    clearTimeout(hideGuideTimer);
    guide.hidden = true;
    help.setAttribute("aria-expanded", "false");
    if (restoreFocus) help.focus({ preventScroll: true });
  }

  function openGuide() {
    if (demo.hidden) return;
    clearTimeout(hideGuideTimer);
    guide.hidden = false;
    help.setAttribute("aria-expanded", "true");
    positionGuide();
  }

  function scheduleHideGuide() {
    clearTimeout(showGuideTimer);
    clearTimeout(hideGuideTimer);
    hideGuideTimer = setTimeout(() => {
      if (!guide.matches(":hover") && !help.matches(":hover")
          && !guide.contains(document.activeElement) && document.activeElement !== help) closeGuide();
    }, 250);
  }

  help.addEventListener("pointerenter", event => {
    if (event.pointerType === "touch") return;
    clearTimeout(hideGuideTimer);
    clearTimeout(showGuideTimer);
    showGuideTimer = setTimeout(openGuide, 2000);
  });
  help.addEventListener("pointerleave", scheduleHideGuide);
  help.addEventListener("click", () => guide.hidden ? openGuide() : closeGuide());
  help.addEventListener("focusout", scheduleHideGuide);
  guide.addEventListener("pointerenter", () => clearTimeout(hideGuideTimer));
  guide.addEventListener("pointerleave", scheduleHideGuide);
  guide.addEventListener("focusout", scheduleHideGuide);
  closeGuideButton.addEventListener("click", () => closeGuide(true));
  document.addEventListener("pointerdown", event => {
    if (!guide.contains(event.target) && !help.contains(event.target)) closeGuide();
  });
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
    if (!guide.hidden) positionGuide();
  }

  function renderTranscript() {
    const steps = words.slice(0, wordCount);
    const enabled = punctuation.getAttribute("aria-pressed") === "true";
    const raw = steps.map(step => step[0]).join("");
    const formatted = steps.reduce((text, step) => step[1] === "\b"
      ? Array.from(text).slice(0, -1).join("") : text + step[1], "");
    transcript.textContent = enabled ? formatted.replace(/\n+/g, " ") : raw;
    transcript.title = enabled ? formatted : raw;
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
    words = samples[next];
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
    const shouldRestoreFocus = demo.contains(document.activeElement) || guide.contains(document.activeElement);
    closeGuide();
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
      if (!guide.hidden) closeGuide(guide.contains(document.activeElement));
      else closeDemo();
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
    closeGuide();
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
