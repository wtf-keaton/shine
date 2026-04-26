import { invoke } from './core.js';
export const appWindow = {
    startDrag: async () => {
        await invoke('window_drag');
    }
};
