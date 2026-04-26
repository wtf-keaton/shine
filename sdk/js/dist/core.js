const pendingRequests = new Map();
let _ipcIdCounter = 0;
if (typeof window !== 'undefined') {
    window.__SHINE_IPC_RECEIVE__ = function (response) {
        if (response.id && pendingRequests.has(response.id)) {
            const { resolve, reject } = pendingRequests.get(response.id);
            if (response.data && response.data.error) {
                reject(new Error(response.data.error));
            }
            else {
                resolve(response.data);
            }
            pendingRequests.delete(response.id);
        }
    };
}
export async function invoke(cmd, payload = {}) {
    return new Promise((resolve, reject) => {
        const requestId = 'req_' + (++_ipcIdCounter);
        pendingRequests.set(requestId, { resolve, reject });
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage({ id: requestId, cmd, payload });
        }
        else {
            console.warn(`[Shine API] Simulated response for command: ${cmd}`);
            reject(new Error("Shine API is only available inside the native app."));
            pendingRequests.delete(requestId);
        }
    });
}
