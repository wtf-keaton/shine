const pendingRequests = new Map();

window.__SHINE_IPC_RECEIVE__ = function(response) {
    if (response.id && pendingRequests.has(response.id)) {
        const { resolve, reject } = pendingRequests.get(response.id);

        if (response.data && response.data.error) {
            reject(new Error(response.data.error));
        } else {
            resolve(response.data);
        }

        pendingRequests.delete(response.id);
    } else {
        console.log("[Shine IPC Unhandled]", response);
    }
};

/**
 * Вызывает C++ команду и возвращает Promise с результатом.
 * @param {string} cmd - Название команды (например, 'fs_read_text_file')
 * @param {object} payload - Аргументы для C++ (например, { path: 'C:/...' })
 * @returns {Promise<any>}
 */
export async function invoke(cmd, payload = {}) {
    return new Promise((resolve, reject) => {
        const requestId = crypto.randomUUID();

        pendingRequests.set(requestId, { resolve, reject });

        const message = {
            id: requestId,
            cmd: cmd,
            payload: payload
        };

        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(message);
        } else {
            reject(new Error("Shine API is only available inside the native app."));
            pendingRequests.delete(requestId);
        }
    });
}