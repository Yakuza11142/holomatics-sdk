/**
 * File: tesseract_webar.js
 * Target: Web Browsers (Safari / Chrome WebAR via WebAssembly)
 * Framework Support: Three.js
 */

class TesseractWebAR {
    constructor(wasmModule) {
        this.wasm = wasmModule;
        this.ctxPtr = this.wasm._tess_init();
        this.matrixBuffer = this.wasm._malloc(16 * 4); // Allocate 64 bytes for 4x4 matrix
        this.cameraMatrix = new Float32Array(16);
        
        this.gyro = { x: 0, y: 0, z: 0 };
        this.accel = { x: 0, y: 0, z: 0 };
        this.lastTimestamp = performance.now();
        
        this.initSensors();
    }

    initSensors() {
        if (window.DeviceMotionEvent) {
            window.addEventListener('devicemotion', (event) => {
                if (event.rotationRate) {
                    this.gyro.x = (event.rotationRate.beta || 0) * (Math.PI / 180.0);
                    this.gyro.y = (event.rotationRate.gamma || 0) * (Math.PI / 180.0);
                    this.gyro.z = (event.rotationRate.alpha || 0) * (Math.PI / 180.0);
                }
                if (event.accelerationIncludingGravity) {
                    this.accel.x = event.accelerationIncludingGravity.x || 0;
                    this.accel.y = event.accelerationIncludingGravity.y || 0;
                    this.accel.z = event.accelerationIncludingGravity.z || 0;
                }
            });
        }
    }

    // Process frame and apply direct 6-DoF transformation matrix to Three.js camera
    update(threeCamera, videoFrameBuffer = null, width = 0, height = 0) {
        const now = performance.now();
        const dt = Math.max((now - this.lastTimestamp) / 1000.0, 0.001);
        this.lastTimestamp = now;

        // Execute WebAssembly C-Engine Loop
        this.wasm._tess_update(
            this.ctxPtr,
            0, // Optional pointer to grayscale WebGL video texture
            width, height,
            this.gyro.x, this.gyro.y, this.gyro.z,
            this.accel.x, this.accel.y, this.accel.z,
            dt
        );

        // Retrieve updated transformation matrix from C WASM heap
        this.wasm._tess_get_matrix(this.ctxPtr, this.matrixBuffer);
        const heapMatrix = new Float32Array(this.wasm.HEAPF32.buffer, this.matrixBuffer, 16);

        // Directly bind position and rotation to Three.js Camera
        threeCamera.matrixAutoUpdate = false;
        threeCamera.matrix.fromArray(heapMatrix);
        threeCamera.matrixWorldNeedsUpdate = true;
    }

    destroy() {
        if (this.ctxPtr) {
            this.wasm._tess_free(this.ctxPtr);
            this.wasm._free(this.matrixBuffer);
        }
    }
}

export default TesseractWebAR;
