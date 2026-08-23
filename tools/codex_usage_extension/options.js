const token = document.querySelector("#token");
const status = document.querySelector("#status");
chrome.storage.local.get({ token: "" }).then((settings) => { token.value = settings.token; });
document.querySelector("#save").addEventListener("click", async () => {
  await chrome.storage.local.set({ token: token.value.trim() });
  status.textContent = "已保存。";
  status.className = "success";
});
