// A drag handle for resizing a site's chat column, shared by the content
// scripts of sites that have no way to do it themselves (lofi-nopixel.com,
// twitch.tv). Chatterino follows the chat's size, so it resizes along with the
// column. The handle sits just outside the column, since Chatterino covers the
// column itself.
globalThis.createChatResizer = ({ widthKey, onResize }) => {
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

  chrome.storage.local.get(widthKey).then(stored => {
    chatWidth = stored[widthKey] ?? null;
    applyWidth();
    onResize();
  });

  /**
   * The chat column: the outermost ancestor of the chat that's lined up with
   * it on both sides (so it also holds the header and the pills).
   * @param {Element} chat
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
    return found ?? chat;
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
      chrome.storage.local.set({ [widthKey]: chatWidth }).catch(() => {});
    };
    grip.addEventListener('pointermove', move);
    grip.addEventListener('pointerup', up);
    grip.addEventListener('pointercancel', up);
  });

  grip.addEventListener('dblclick', () => {
    chatWidth = null;
    applyWidth();
    chrome.storage.local.remove(widthKey).catch(() => {});
    onResize();
  });

  return {
    /** @param {Element | null} chat the shown chat, if any */
    follow(chat) {
      const found = chat && findPanel(chat);
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
    },
  };
};
