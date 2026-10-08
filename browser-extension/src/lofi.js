// Multi-POV support for lofi-nopixel.com.
//
// Reports which POVs currently have their chat open, so Chatterino can keep
// its "Multi-POV" tab in sync. POVs are reported as the site's own slugs
// ("t-xqc" for Twitch, "k-xqc" for Kick); Chatterino maps both to the Twitch
// channel of the same name.
(() => {
  const DEBOUNCE_MS = 150;

  // Chat embeds the site opens when a POV's chat is toggled on
  const chatFrameMatchers = [
    // https://www.twitch.tv/embed/<name>/chat?parent=...
    { prefix: 't-', re: /^https:\/\/(?:www\.)?twitch\.tv\/embed\/(\w+)\/chat/ },
    // https://kick.com/popout/<name>/chat
    {
      prefix: 'k-',
      re: /^https:\/\/(?:www\.)?kick\.com\/popout\/([\w-]+)\/chat/,
    },
  ];

  let lastSent = null;
  let timer = 0;

  function isMultiPov() {
    return location.pathname.startsWith('/multipov');
  }

  /** @returns {string[]} slugs of POVs with chat open, in page order */
  function findOpenChats() {
    const povs = [];
    for (const frame of document.getElementsByTagName('iframe')) {
      const src = frame.src;
      for (const { prefix, re } of chatFrameMatchers) {
        const match = src.match(re);
        if (match) {
          const slug = prefix + match[1].toLowerCase();
          if (!povs.includes(slug)) povs.push(slug);
          break;
        }
      }
    }
    return povs;
  }

  function send(force = false) {
    timer = 0;
    // Leaving the Multi-POV page doesn't clear the tab, so the last set of
    // chats stays open while browsing the rest of the site.
    if (!isMultiPov()) return;

    const povs = findOpenChats();
    const key = povs.join(',');
    if (!force && key === lastSent) return;
    lastSent = key;

    try {
      chrome.runtime.sendMessage({ type: 'multipov', povs });
    } catch (err) {
      // Extension was reloaded; this content script is orphaned
      observer.disconnect();
    }
  }

  function schedule() {
    if (!timer) timer = setTimeout(send, DEBOUNCE_MS);
  }

  // Chats are toggled by adding/removing iframes (or swapping their src), so
  // one observer on the body catches every toggle and SPA navigation.
  const observer = new MutationObserver(schedule);
  observer.observe(document.body, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ['src'],
  });

  // Coming back to this tab: re-apply its chats, another tab may have changed
  // them in the meantime.
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') send(true);
  });
  window.addEventListener('focus', () => send(true));

  schedule();
})();
