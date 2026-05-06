type PendingRequest = {
    resolve: (value: any) => void;
    reject: (reason?: any) => void;
    timeoutId: ReturnType<typeof setTimeout>;
};

const REQUEST_TIMEOUT_MS = 30_000;
const pendingRequests = new Map<string, PendingRequest>();
let _ipcIdCounter = 0;

declare global {
    interface Window {
        __SHINE_IPC_RECEIVE__: (response: any) => void;
        chrome?: {
            webview?: {
                postMessage: (message: any) => void;
                addEventListener?: (type: 'message', listener: (event: { data: any }) => void) => void;
            }
        };
    }
}

function normalizeResponse(response: any): any | null {
    if (typeof response !== 'string') {
        return response;
    }

    try {
        return JSON.parse(response);
    } catch {
        return null;
    }
}

if (typeof window !== 'undefined') {
    window.__SHINE_IPC_RECEIVE__ = function(response: any) {
        response = normalizeResponse(response);
        if (response?.id && pendingRequests.has(response.id)) {
            const { resolve, reject, timeoutId } = pendingRequests.get(response.id)!;
            pendingRequests.delete(response.id);
            clearTimeout(timeoutId);

            if (response.data && response.data.error) {
                reject(new Error(response.data.error));
            } else {
                resolve(response.data);
            }
        }
    };

    window.chrome?.webview?.addEventListener?.('message', (event) => {
        window.__SHINE_IPC_RECEIVE__(event.data);
    });
}

export async function invoke<T>(cmd: string, payload: any = {}): Promise<T> {
    return new Promise((resolve, reject) => {
        const requestId = 'req_' + (++_ipcIdCounter);
        const timeoutId = setTimeout(() => {
            pendingRequests.delete(requestId);
            reject(new Error(`Shine command timed out: ${cmd}`));
        }, REQUEST_TIMEOUT_MS);

        pendingRequests.set(requestId, { resolve, reject, timeoutId });

        if (typeof window !== 'undefined' && window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage({ id: requestId, cmd, payload });
        } else {
            console.warn(`[Shine API] Simulated response for command: ${cmd}`);
            clearTimeout(timeoutId);
            reject(new Error("Shine API is only available inside the native app."));
            pendingRequests.delete(requestId);
        }
    });
}
