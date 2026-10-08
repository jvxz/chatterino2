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
  // loading messages and emotes nobody sees. Its URL is kept on the frame.
  const BLANK = 'about:blank';

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

  function frameSrc(frame) {
    return frame.src === BLANK
      ? frame.dataset.chatterinoSrc ?? BLANK
      : frame.src;
  }

  function unloadChat(frame) {
    if (frame.src === BLANK) return;
    frame.dataset.chatterinoSrc = frame.src;
    frame.src = BLANK;
  }

  function povFromFrame(frame) {
    const src = frameSrc(frame);
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

  function post(message) {
    try {
      chrome.runtime.sendMessage(message);
    } catch {
      // Extension was reloaded; this content script is orphaned
      clearInterval(timer);
    }
  }

  function hideOverlay() {
    if (lastOverlay === '') return;
    lastOverlay = '';
    post({ type: 'overlay', pov: null });
  }

  function update(force = false) {
    if (
      !isMultiPov() ||
      document.visibilityState !== 'visible' ||
      document.fullscreenElement
    ) {
      hideOverlay();
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

    if (!shown) {
      hideOverlay();
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
  window.addEventListener('pagehide', hideOverlay);

  update(true);
})();
