async function getSettings() {
  return chrome.storage.local.get({ token: "", automatic: false });
}

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "pushUsage") return;
  (async () => {
    const { token } = await getSettings();
    const headers = { "Content-Type": "application/json" };
    if (token) headers["X-Badge-Token"] = token;
    const response = await fetch("http://127.0.0.1:8765/v1/codex-usage", {
      method: "POST",
      headers,
      body: JSON.stringify({ percent: message.percent, window: message.window })
    });
    const body = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(body.message || `本机转发器返回 ${response.status}`);
    sendResponse({ ok: true, message: body.message || "已同步" });
  })().catch((error) => sendResponse({ ok: false, message: error.message }));
  return true;
});
