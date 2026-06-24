document.getElementById('sendRpcBtn').addEventListener('click', async () => {
    const inputVal = document.getElementById('rpcInput').value;
    const resultElement = document.getElementById('rpcResult');

    resultElement.textContent = 'Sending...';

    try {
        // Call the C++ RPC handler registered via bind().
        if (window.callCppRPC) {
            const cppResponse = await window.callCppRPC(inputVal);
            resultElement.textContent = cppResponse;
        } else {
            resultElement.textContent = 'Error: window.callCppRPC is not bound!';
            resultElement.style.color = '#f87171';
        }
    } catch (err) {
        resultElement.textContent = 'RPC failed: ' + err;
        resultElement.style.color = '#f87171';
    }
});
