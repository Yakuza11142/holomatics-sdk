let tessInstance = null;
let tessCreate, tessDestroy, tessProcess;

Module.onRuntimeInitialized = () => {
    // Wrap low-level C functions
    tessCreate = Module.cwrap('tess_create', 'number', []);
    tessDestroy = Module.cwrap('tess_destroy', 'void', ['number']);
    tessProcess = Module.cwrap('tess_process_frame', 'number', ['number', 'number']);

    // Initialize engine in web browser
    tessInstance = tessCreate();
    console.log("Tesseract Engine loaded natively on Web!");
};

function renderLoop(deltaTime) {
    if (tessInstance) {
        const status = tessProcess(tessInstance, deltaTime);
        if (status !== 0) console.error("Tesseract Engine error code:", status);
    }
}
