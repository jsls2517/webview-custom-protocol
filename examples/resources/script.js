document.getElementById('sendRpcBtn').addEventListener('click', async () => {
    const inputVal = document.getElementById('rpcInput').value;
    const resultElement = document.getElementById('rpcResult');
    
    resultElement.textContent = '正在发送...';
    
    try {
        // 调用绑定的 C++ RPC 函数
        if (window.callCppRPC) {
            const cppResponse = await window.callCppRPC(inputVal);
            resultElement.textContent = cppResponse;
        } else {
            resultElement.textContent = '错误：window.callCppRPC 未绑定！';
            resultElement.style.color = '#f87171';
        }
    } catch (err) {
        resultElement.textContent = 'RPC 失败: ' + err;
        resultElement.style.color = '#f87171';
    }
});
