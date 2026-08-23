const status = document.querySelector("#status");
const sync = document.querySelector("#sync");
const automatic = document.querySelector("#automatic");

function show(message, kind = "") {
  status.textContent = message;
  status.className = kind;
}

async function activeTab() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id || !tab.url?.startsWith("https://chatgpt.com/")) {
    throw new Error("请切换到已登录的 ChatGPT 用量页面。");
  }
  return tab;
}

async function syncUsage() {
  sync.disabled = true;
  show("正在读取当前页面的可见额度…");
  try {
    const tab = await activeTab();
    let result;
    try {
      result = await chrome.tabs.sendMessage(tab.id, { type: "collectUsage" });
    } catch (error) {
      // Chrome only injects declared content scripts after a navigation.  When
      // an extension has just been loaded, inject it for this user-initiated
      // sync instead of requiring a manual page reload.
      if (!String(error.message).includes("Receiving end does not exist")) throw error;
      await chrome.scripting.executeScript({ target: { tabId: tab.id }, files: ["content.js"] });
      result = await chrome.tabs.sendMessage(tab.id, { type: "collectUsage" });
    }
    if (!result?.ok) throw new Error(result?.message || "同步失败");
    show(`已同步 ${result.percent}% 到徽章。`, "success");
  } catch (error) {
    show(error.message, "error");
  } finally {
    sync.disabled = false;
  }
}

chrome.storage.local.get({ automatic: false }).then(({ automatic: enabled }) => { automatic.checked = enabled; });
sync.addEventListener("click", () => { void syncUsage(); });
automatic.addEventListener("change", () => chrome.storage.local.set({ automatic: automatic.checked }));
document.querySelector("#settings").addEventListener("click", () => chrome.runtime.openOptionsPage());
