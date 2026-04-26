declare global {
    interface Window {
        __SHINE_IPC_RECEIVE__: (response: any) => void;
        chrome?: {
            webview?: {
                postMessage: (message: any) => void;
            };
        };
    }
}
export declare function invoke<T>(cmd: string, payload?: any): Promise<T>;
