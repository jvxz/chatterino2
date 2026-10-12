// Drag handle that resizes a page's chat column, shared by the content scripts
// (inject.js for Twitch, lofi.js for lofi-nopixel.com).
//
// Neither site lets you resize its chat, so a handle on the column's left edge
// does. Chatterino follows the chat's size on the page, so this resizes it
// too. The handle sits just outside the column, since Chatterino covers the
// column.
globalThis.createChatResizer = ({ storageKey, onResize }) => {
  const MIN_CHAT_WIDTH = 200;
  const GRIP_WIDTH = 8;

  /** @type {number | null} width the column was dragged to, in CSS pixels */
  let chatWidth = null;
  /** @type {HTMLElement | null} */
  let panel = null;
  // While suspended (the column is collapsed or below the video) the page's
  // own width is left alone
  let suspended = false;

  const grip = document.createElement('div');
  grip.title = 'Drag to resize the chat, double-click to reset';
  grip.style.cssText = `position: fixed; z-index: 2147483647; width: ${GRIP_WIDTH}px; cursor: col-resize; display: none;`;
  grip.addEventListener('pointerenter', () => {
    grip.style.background = 'rgba(255, 255, 255, 0.15)';
  });
  grip.addEventListener('pointerleave', () => {
    grip.style.background = '';
  });

  chrome.storage.local.get(storageKey).then(stored => {
    chatWidth = stored[storageKey] ?? null;
    applyWidth();
    onResize();
  });

  /**
   * The chat column: the outermost ancestor of the chat that's lined up with
   * it on both sides (so it also holds the header and the pills).
   * @param {HTMLElement} chat
   */
  function findPanel(chat) {
    const chatRect = chat.getBoundingClientRect();
    let found = null;
    for (
      let el = chat.parentElement;
      el && el !== document.body;
      el = el.parentElement
    ) {
      const rect = el.getBoundingClientRect();
      if (
        Math.abs(rect.left - chatRect.left) > 2 ||
        Math.abs(rect.right - chatRect.right) > 2
      ) {
        break;
      }
      found = el;
    }
    return found;
  }

  /** @type {HTMLElement | null} the element carrying our width, if any */
  let styled = null;
  const WIDTH_PROPS = ['width', 'min-width', 'max-width', 'flex'];

  function applyWidth() {
    if (styled && (styled !== panel || chatWidth === null || suspended)) {
      // Only undo what was set here, the site may have its own inline styles
      for (const prop of WIDTH_PROPS) styled.style.removeProperty(prop);
      styled = null;
    }
    if (!panel || chatWidth === null || suspended) return;

    const width = `${chatWidth}px`;
    panel.style.setProperty('width', width, 'important');
    panel.style.setProperty('min-width', width, 'important');
    panel.style.setProperty('max-width', width, 'important');
    panel.style.setProperty('flex', `0 0 ${width}`, 'important');
    styled = panel;
  }

  /**
   * Keeps the handle on the chat column. Call it on every poll.
   * @param {HTMLElement | null} chat the shown chat, if any
   * @param {boolean} [active] false while the column shouldn't be resized,
   *   it then keeps the page's own width
   */
  function follow(chat, active = true) {
    if (!chat) {
      panel = null;
      grip.style.display = 'none';
      return;
    }

    // A collapsed column can't be measured, so it keeps the panel it had
    const found = active ? findPanel(chat) : panel;
    if (found !== panel || suspended === active) {
      panel = found;
      suspended = !active;
      applyWidth();
    }
    if (!panel || suspended) {
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
      onResize();
    };
    const up = () => {
      grip.removeEventListener('pointermove', move);
      grip.removeEventListener('pointerup', up);
      grip.removeEventListener('pointercancel', up);
      chrome.storage.local.set({ [storageKey]: chatWidth }).catch(() => {});
    };
    grip.addEventListener('pointermove', move);
    grip.addEventListener('pointerup', up);
    grip.addEventListener('pointercancel', up);
  });

  grip.addEventListener('dblclick', () => {
    chatWidth = null;
    applyWidth();
    chrome.storage.local.remove(storageKey).catch(() => {});
    onResize();
  });

  return {
    follow,
    /** @returns {HTMLElement | null} the column being resized */
    get panel() {
      return panel;
    },
  };
};
