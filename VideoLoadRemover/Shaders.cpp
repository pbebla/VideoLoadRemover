#include "Shaders.h"

bool VertexShader::Initialize(winrt::com_ptr<ID3D11Device>& device, std::wstring shaderpath)
{
    HRESULT hr = D3DReadFileToBlob(shaderpath.c_str(), shader_buffer.put());
    if (FAILED(hr)) {
        DBOUT("failed to load shader" << shaderpath.c_str());
        return false;
    }
    hr = device->CreateVertexShader(shader_buffer->GetBufferPointer(), shader_buffer->GetBufferSize(), NULL, shader.put());
    if (FAILED(hr)) {
        DBOUT("failed to create vertex shader" << shaderpath.c_str());
        return false;
    }
    return true;
}

ID3D11VertexShader* VertexShader::GetShader()
{
    return shader.get();
}

ID3D10Blob* VertexShader::GetBuffer()
{
    return shader_buffer.get();
}

bool PixelShader::Initialize(winrt::com_ptr<ID3D11Device>& device, std::wstring shaderpath)
{
    HRESULT hr = D3DReadFileToBlob(shaderpath.c_str(), shader_buffer.put());
    if (FAILED(hr)) {
        DBOUT("failed to load shader" << shaderpath.c_str());
        return false;
    }
    hr = device->CreatePixelShader(shader_buffer->GetBufferPointer(), shader_buffer->GetBufferSize(), NULL, shader.put());
    if (FAILED(hr)) {
        DBOUT("failed to create pixel shader" << shaderpath.c_str());
        return false;
    }
    return true;
}

ID3D11PixelShader* PixelShader::GetShader()
{
    return shader.get();
}

ID3D10Blob* PixelShader::GetBuffer()
{
    return shader_buffer.get();
}
