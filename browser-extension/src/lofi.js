// Multi-POV support for lofi-nopixel.com.
//
// Tells Chatterino which chat the page is showing and where it is on screen,
// so Chatterino can sit on top of it ("overlay"), plus which POVs the page
// has open ("multipov", for the optional Multi-POV tab). POVs are reported
// as the site's own slugs ("t-xqc" for Twitch, "k-xqc" for Kick); Chatterino
// maps both to the Twitch channel of the same name.
(() => {
  // How often the chat's position is checked. Moving or resizing the browser
  // window doesn't fire any event inside the page, so this has to poll.
  const POLL_MS = 100;

  // Chat embeds the site shows. Only the selected POV's chat is in the page.
  const chatFrameMatchers = [
    // https://chat.kick.cx/embed/<name>, what the site uses for Kick POVs
    { prefix: 'k-', re: /^https:\/\/chat\.kick\.cx\/embed\/([\w-]+)/ },
    // https://kick.com/popout/<name>/chat
    {
      prefix: 'k-',
      re: /^https:\/\/(?:www\.)?kick\.com\/popout\/([\w-]+)\/chat/,
    },
    // https://www.twitch.tv/embed/<name>/chat?parent=...
    { prefix: 't-', re: /^https:\/\/(?:www\.)?twitch\.tv\/embed\/(\w+)\/chat/ },
  ];

  // The site's own chat is unloaded while Chatterino covers it, so it stops
  // loading messages and emotes nobody sees. A srcdoc replaces the page in the
  // frame but leaves its src alone, so the chat can still be told apart. The
  // empty frame takes the background of Chatterino's chat, which Chatterino
  // reports through the background page.
  const BACKGROUND_KEY = 'chatterinoBackground';
  let background = 'transparent';

  chrome.storage.local.get(BACKGROUND_KEY).then(stored => {
    background = stored[BACKGROUND_KEY] ?? background;
    update(true);
  });
  chrome.storage.onChanged.addListener((changes, area) => {
    if (area === 'local' && changes[BACKGROUND_KEY]?.newValue) {
      background = changes[BACKGROUND_KEY].newValue;
      update(true);
    }
  });

  let lastOverlay = null;
  let lastPovs = null;

  function isMultiPov() {
    return location.pathname.startsWith('/multipov');
  }

  /**
   * POVs open on the page, from its URL: /multipov/k-anthonyz/t-xqc
   * @returns {string[]}
   */
  function povsFromUrl() {
    const povs = [];
    for (const segment of location.pathname.split('/').slice(2)) {
      if (!segment) continue;
      const pov = decodeURIComponent(segment).toLowerCase();
      if (!povs.includes(pov)) povs.push(pov);
    }
    return povs;
  }

  function unloadChat(frame) {
    // color-scheme matches the site's, or Chrome paints the frame white
    const srcdoc = `<style>:root { color-scheme: dark; background: ${background}; }</style>`;
    if (frame.srcdoc !== srcdoc) frame.srcdoc = srcdoc;
  }

  function povFromFrame(frame) {
    const src = frame.src;
    for (const { prefix, re } of chatFrameMatchers) {
      const match = src.match(re);
      if (match) return prefix + match[1].toLowerCase();
    }
    return null;
  }

  /**
   * @returns {{ pov: string, frame: HTMLIFrameElement, rect: DOMRect } | null}
   *   the largest visible chat embed
   */
  function findShownChat() {
    let shown = null;
    let shownArea = 0;
    for (const frame of document.getElementsByTagName('iframe')) {
      const pov = povFromFrame(frame);
      if (!pov) continue;

      const rect = frame.getBoundingClientRect();
      const area = rect.width * rect.height;
      if (area > shownArea) {
        shown = { pov, frame, rect };
        shownArea = area;
      }
    }
    return shown;
  }

  // The site has no way to resize its chat column, so a handle on its left edge
  // does. Chatterino follows the chat's size, so this resizes it too. The
  // handle sits just outside the column, since Chatterino covers the column.
  const WIDTH_KEY = 'lofiChatWidth';
  const MIN_CHAT_WIDTH = 200;
  const GRIP_WIDTH = 8;

  /** @type {number | null} width the column was dragged to, in CSS pixels */
  let chatWidth = null;
  /** @type {HTMLElement | null} */
  let panel = null;
  const grip = document.createElement('div');
  grip.title = 'Drag to resize the chat, double-click to reset';
  grip.style.cssText = `position: fixed; z-index: 2147483647; width: ${GRIP_WIDTH}px; cursor: col-resize; display: none;`;
  grip.addEventListener('pointerenter', () => {
    grip.style.background = 'rgba(255, 255, 255, 0.15)';
  });
  grip.addEventListener('pointerleave', () => {
    grip.style.background = '';
  });

  chrome.storage.local.get(WIDTH_KEY).then(stored => {
    chatWidth = stored[WIDTH_KEY] ?? null;
    applyWidth();
  });

  /**
   * The chat column: the outermost ancestor of the chat's iframe that's lined
   * up with it on both sides (so it also holds the header and the pills).
   * @param {HTMLIFrameElement} frame
   */
  function findPanel(frame) {
    const frameRect = frame.getBoundingClientRect();
    let found = null;
    for (
      let el = frame.parentElement;
      el && el !== document.body;
      el = el.parentElement
    ) {
      const rect = el.getBoundingClientRect();
      if (
        Math.abs(rect.left - frameRect.left) > 2 ||
        Math.abs(rect.right - frameRect.right) > 2
      ) {
        break;
      }
      found = el;
    }
    return found;
  }

  function applyWidth() {
    if (!panel) return;
    if (chatWidth === null) {
      for (const prop of ['width', 'min-width', 'max-width', 'flex']) {
        panel.style.removeProperty(prop);
      }
      return;
    }
    const width = `${chatWidth}px`;
    panel.style.setProperty('width', width, 'important');
    panel.style.setProperty('min-width', width, 'important');
    panel.style.setProperty('max-width', width, 'important');
    panel.style.setProperty('flex', `0 0 ${width}`, 'important');
  }

  /** @param {HTMLIFrameElement | null} frame the shown chat, if any */
  function followPanel(frame) {
    const found = frame && findPanel(frame);
    if (found !== panel) {
      panel = found;
      applyWidth();
    }
    if (!panel) {
      grip.style.display = 'none';
      return;
    }
    if (!grip.isConnected) document.documentElement.append(grip);
    const rect = panel.getBoundingClientRect();
    grip.style.left = `${rect.left - GRIP_WIDTH}px`;
    grip.style.top = `${rect.top}px`;
    grip.style.height = `${rect.height}px`;
    grip.style.display = '';
  }

  grip.addEventListener('pointerdown', down => {
    if (!panel || down.button !== 0) return;
    down.preventDefault();
    grip.setPointerCapture(down.pointerId);
    const startX = down.clientX;
    const startWidth = panel.getBoundingClientRect().width;

    const move = event => {
      const maxWidth = Math.max(MIN_CHAT_WIDTH, window.innerWidth * 0.75);
      chatWidth = Math.round(
        Math.min(
          maxWidth,
          Math.max(MIN_CHAT_WIDTH, startWidth + startX - event.clientX),
        ),
      );
      applyWidth();
      update();
    };
    const up = () => {
      grip.removeEventListener('pointermove', move);
      grip.removeEventListener('pointerup', up);
      grip.removeEventListener('pointercancel', up);
      chrome.storage.local.set({ [WIDTH_KEY]: chatWidth }).catch(() => {});
    };
    grip.addEventListener('pointermove', move);
    grip.addEventListener('pointerup', up);
    grip.addEventListener('pointercancel', up);
  });

  grip.addEventListener('dblclick', () => {
    chatWidth = null;
    applyWidth();
    chrome.storage.local.remove(WIDTH_KEY).catch(() => {});
    update();
  });

  function post(message) {
    try {
      chrome.runtime.sendMessage(message);
    } catch {
      // Extension was reloaded; this content script is orphaned
      clearInterval(timer);
    }
  }

  /** @param {string} reason for Chatterino's logs */
  function hideOverlay(reason) {
    if (lastOverlay === '') return;
    lastOverlay = '';
    post({ type: 'overlay', pov: null, reason });
  }

  /** @returns {string | null} why the overlay can't be shown right now */
  function hiddenReason() {
    if (!isMultiPov()) return 'not on the Multi-POV page';
    if (document.visibilityState !== 'visible') return 'tab hidden';
    if (document.fullscreenElement) return 'page fullscreen';
    return null;
  }

  function update(force = false) {
    const reason = hiddenReason();
    if (reason) {
      followPanel(null);
      hideOverlay(reason);
      return;
    }

    const povs = povsFromUrl();
    const shown = findShownChat();

    // Leaving the Multi-POV page doesn't clear the tab, so the last set of
    // chats stays open while browsing the rest of the site.
    const povsKey = povs.join(',');
    if (force || povsKey !== lastPovs) {
      lastPovs = povsKey;
      post({ type: 'multipov', povs });
    }

    followPanel(shown?.frame ?? null);
    if (!shown) {
      hideOverlay('no chat on the page');
      return;
    }

    const { x, y, width, height } = shown.rect;
    const overlay = {
      type: 'overlay',
      pov: shown.pov,
      rect: { x, y, width, height },
      viewport: {
        screenX: window.screenX,
        screenY: window.screenY,
        outerWidth: window.outerWidth,
        outerHeight: window.outerHeight,
        innerWidth: window.innerWidth,
        innerHeight: window.innerHeight,
      },
    };
    const overlayKey = JSON.stringify(overlay);
    if (force || overlayKey !== lastOverlay) {
      lastOverlay = overlayKey;
      post(overlay);
    }
    unloadChat(shown.frame);
  }

  const timer = setInterval(update, POLL_MS);

  // Switching to another tab or app hides the overlay, coming back shows it
  // again right away.
  document.addEventListener('visibilitychange', () => update(true));
  window.addEventListener('focus', () => update(true));
  window.addEventListener('pagehide', () => hideOverlay('page closed'));

  update(true);
})();
