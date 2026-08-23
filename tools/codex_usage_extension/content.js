function parseVisibleUsage() {
  // Only derive the percentage and its time window from rendered text.  The
  // original page text, account name, cookies, and network data never leave this script.
  const text = document.body?.innerText || "";
  const patterns = [
    /(?:5\s*(?:hour|小时)[^\n]{0,80}?)(\d{1,3})\s*%/iu,
    /(?:weekly|week|本周|每周)[^\n]{0,80}?(\d{1,3})\s*%/iu,
    /(\d{1,3})\s*%\s*(?:remaining|left|可用|剩余)/iu,
    /(?:remaining|left|可用|剩余)[^\n]{0,40}?(\d{1,3})\s*%/iu
  ];
  for (let index = 0; index < patterns.length; index += 1) {
    const match = text.match(patterns[index]);
    if (!match) continue;
    const percent = Number(match[1]);
    if (Number.isInteger(percent) && percent >= 0 && percent <= 100) {
      return { ok: true, percent, window: index === 0 ? "five_hour" : index === 1 ? "weekly" : "unknown" };
    }
  }
  return { ok: false, message: "未在当前页面找到可见的代码助手额度。请打开“设置 → 用量”后重试。" };
}

async function collectAndPush() {
  const usage = parseVisibleUsage();
  if (!usage.ok) return usage;
  const result = await chrome.runtime.sendMessage({ type: "pushUsage", ...usage });
  return result.ok ? { ...usage, ...result } : result;
}

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "collectUsage") return;
  collectAndPush().then(sendResponse).catch((error) => sendResponse({ ok: false, message: error.message }));
  return true;
});

// Automatic mode only acts after the user opted in and only while a ChatGPT
// page containing a recognizable visible usage percentage remains open.
let debounceTimer;
async function tryAutomaticSync() {
  const { automatic } = await chrome.storage.local.get({ automatic: false });
  if (automatic) await collectAndPush();
}

new MutationObserver(() => {
  clearTimeout(debounceTimer);
  debounceTimer = setTimeout(() => { void tryAutomaticSync(); }, 1200);
}).observe(document.documentElement, { childList: true, subtree: true, characterData: true });

void tryAutomaticSync();
