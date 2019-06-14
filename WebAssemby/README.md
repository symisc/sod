The article depicting the whole process of porting the <a href="https://sod.pixlab.io/c_api/sod_realnet_detect.html">SOD realnets face detector</a> to WebAssembly is available to consult at: <a href="https://sod.pixlab.io/articles/porting-c-face-detector-webassembly.html">Porting a Face Detector Written in C to WebAssembly</a>.

This frontal face detector, **WebAssemby model** is pre-trained on the Genki-4K datatset for **Web oriented applications**.
The model is production ready, **works at Real-Time on all modern browsers (mobile devices included)**. Usage instruction already included in the package.

The model must be downloaded from https://pixlab.io/downloads. Once downloaded, just put it on the directory where the HTML file `usage.html` reside.
     
When you deploy the Webassembly face model on your server, make sure
your HTTP server (Apache, Nginx, etc.) return the appropriate MIME type
for the `wasm` file extension. Under Apache, simply put the following
directives on your .htaccess or Virtual host configuration:

**AddType application/wasm .wasm**

**AddOutputFilterByType DEFLATE application/wasm**


For chrome users, you must test the model on an actual web server, whether served locally (i.e http://127.0.0.1) or remotely. 
This is due to the fact that chrome does not allow WebAssembly modules to be loaded directly from the file system (Edge and Firefox do not have such issue).
