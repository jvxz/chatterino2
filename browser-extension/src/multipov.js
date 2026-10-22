// Multi-POV support for lofi-nopixel.com and gtaservers.org.
//
// Tells Chatterino which chat the page is showing and where it is on screen,
// so Chatterino can sit on top of it ("overlay"), plus which POVs the page
// has open ("multipov", for the optional Multi-POV tab). POVs are reported
// as lofi-nopixel's slugs ("t-xqc" for Twitch, "k-xqc" for Kick) on both
// sites; Chatterino maps both to the Twitch channel of the same name.
(() => {
  // How often the chat's position is checked. Moving or resizing the browser
  // window doesn't fire any event inside the page, so this has to poll.
  const POLL_MS = 100;

  const sites = {
    'lofi-nopixel.com': {
      isMultiPov: () => location.pathname.startsWith('/multipov'),
      povs: povsFromPath,
      findChat: findChatFrame,
      widthKey: 'lofiChatWidth',
    },
    'gtaservers.org': {
      // The NoPixel map and its Multi-POV viewer share one page
      isMultiPov: () => location.pathname.startsWith('/nopixel'),
      povs: povsFromCaptions,
      findChat: findDrawnChat,
      // No widthKey: the site has its own handle (.np4-chat-grip) left of the
      // chat column, outside what Chatterino covers. A width set from outside
      // doesn't hold, as the site places the column itself.
    },
  };
  const site = sites[location.hostname.replace(/^www\./, '')];
  if (!site) return;

  // Chat embeds lofi-nopixel shows. Only the selected POV's chat is in the page.
  const chatFrameMatchers = [
    // https://chat.kick.cx/embed/<name>, what lofi-nopixel uses for Kick POVs
    { prefix: 'k-', re: /^https:\/\/chat\.kick\.cx\/embed\/([\w-]+)/ },
    // https://kick.com/popout/<name>/chat
    {
      prefix: 'k-',
      re: /^https:\/\/(?:www\.)?kick\.com\/popout\/([\w-]+)\/chat/,
    },
    // https://www.twitch.tv/embed/<name>/chat?parent=...
    { prefix: 't-', re: /^https:\/\/(?:www\.)?twitch\.tv\/embed\/(\w+)\/chat/ },
  ];

  // lofi-nopixel's chat is unloaded while Chatterino covers it, so it stops
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

  /**
   * POVs open on the page, from the URL path: /multipov/k-anthonyz/t-xqc
   * @returns {string[]}
   */
  function povsFromPath() {
    const povs = [];
    for (const segment of location.pathname.split('/').slice(2)) {
      if (!segment) continue;
      const pov = decodeURIComponent(segment).toLowerCase();
      if (!povs.includes(pov)) povs.push(pov);
    }
    return povs;
  }

  /**
   * POVs open on gtaservers.org. It keeps its selection in an encoded #v= hash
   * and plays Kick streams itself, so its players (.np4-frame) name no
   * channel. Each player's caption links to the streamer's channels; the
   * selected player has no caption, its links are in the strip below it.
   * @returns {string[]}
   */
  function povsFromCaptions() {
    const strip = document.querySelector('[data-np4-strip]');
    const povs = [];
    for (const player of document.querySelectorAll('.np4-frame')) {
      const caption = document.querySelector(
        `[data-np4-cap="${CSS.escape(player.dataset.np4Frame ?? '')}"]`,
      );
      const pov = channelPov(caption ?? strip, player.dataset.platform);
      if (pov && !povs.includes(pov)) povs.push(pov);
    }
    return povs;
  }

  /**
   * @param {Element | null} links holder of gtaservers.org's channel links:
   *   <a data-np4-channel="kick" href="https://kick.com/<name>">
   * @param {string | undefined} platform the link to prefer, "twitch" or "kick"
   * @returns {string | null}
   */
  function channelPov(links, platform) {
    const link =
      links?.querySelector(
        `a[data-np4-channel="${platform}"]:not([hidden])`,
      ) ?? links?.querySelector('a[data-np4-channel]:not([hidden])');
    // Twitch links point at the VOD instead while one is played back
    const login = link?.href.match(
      /^https:\/\/(?:www\.)?(?:twitch\.tv|kick\.com)\/(?!videos\/)([\w-]+)/,
    )?.[1];
    if (!login) return null;
    const prefix = link.dataset.np4Channel === 'kick' ? 'k-' : 't-';
    return prefix + login.toLowerCase();
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
   * @typedef {{ pov: string, el: HTMLElement, rect: DOMRect }} ShownChat
   * @returns {ShownChat | null} the largest visible chat embed
   */
  function findChatFrame() {
    let shown = null;
    let shownArea = 0;
    for (const frame of document.getElementsByTagName('iframe')) {
      const pov = povFromFrame(frame);
      if (!pov) continue;

      const rect = frame.getBoundingClientRect();
      const area = rect.width * rect.height;
      if (area > shownArea) {
        shown = { pov, el: frame, rect };
        shownArea = area;
      }
    }
    if (!shown) logFrames();
    return shown;
  }

  /**
   * gtaservers.org draws the selected stream's chat itself, in the chat
   * column (aside[data-np4-chat]), so there's no frame to read a channel from.
   * The chat's class names its platform, merged Twitch and Kick chats count as
   * Twitch, and the strip's link for that platform names the channel.
   * @returns {ShownChat | null}
   */
  function findDrawnChat() {
    const chat = document.querySelector('[data-np4-chat] [data-np4-rchat]');
    if (!chat) return null;
    const rect = chat.getBoundingClientRect();
    if (!rect.width || !rect.height) return null;

    const platform = chat.classList.contains('np4-rchat--kick')
      ? 'kick'
      : 'twitch';
    const pov = channelPov(
      document.querySelector('[data-np4-strip]'),
      platform,
    );
    return pov ? { pov, el: chat, rect } : null;
  }

  // lofi-nopixel has no way to resize its chat column, so a handle on its left
  // edge does, see chat-resize.js.
  const resizer = site.widthKey
    ? createChatResizer({
        widthKey: site.widthKey,
        onResize: () => update(),
      })
    : null;

  // When no chat is found, the page's frames go to its console, so a site
  // that changed its chat embed can be matched again
  let lastFrames = null;
  function logFrames() {
    const frames = Array.from(
      document.getElementsByTagName('iframe'),
      f => f.src,
    );
    const key = frames.join(' ');
    if (key === lastFrames) return;
    lastFrames = key;
    console.debug('[Chatterino] no known chat embed among frames:', frames);
  }

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
    if (!site.isMultiPov()) return 'not on the Multi-POV page';
    if (document.visibilityState !== 'visible') return 'tab hidden';
    if (document.fullscreenElement) return 'page fullscreen';
    return null;
  }

  function update(force = false) {
    const reason = hiddenReason();
    if (reason) {
      resizer?.follow(null);
      hideOverlay(reason);
      return;
    }

    const povs = site.povs();
    const shown = site.findChat();

    // Leaving the Multi-POV page doesn't clear the tab, so the last set of
    // chats stays open while browsing the rest of the site.
    const povsKey = povs.join(',');
    if (force || povsKey !== lastPovs) {
      lastPovs = povsKey;
      post({ type: 'multipov', povs });
    }

    resizer?.follow(shown?.el ?? null);
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
    if (shown.el instanceof HTMLIFrameElement) unloadChat(shown.el);
  }

  const timer = setInterval(update, POLL_MS);

  // Switching to another tab or app hides the overlay, coming back shows it
  // again right away.
  document.addEventListener('visibilitychange', () => update(true));
  window.addEventListener('focus', () => update(true));
  window.addEventListener('pagehide', () => hideOverlay('page closed'));

  update(true);
})();
