#pragma once
#pragma comment (lib, "D3DCompiler.lib")
#include "pch.h"
#include <d3dcompiler.h>
class VertexShader
{
public:
	bool Initialize(winrt::com_ptr<ID3D11Device>& device, std::wstring shaderpath);
	ID3D11VertexShader* GetShader();
	ID3D10Blob* GetBuffer();
private:
	winrt::com_ptr<ID3D11VertexShader> shader;
	winrt::com_ptr<ID3D10Blob> shader_buffer;
};

class PixelShader
{
public:
	bool Initialize(winrt::com_ptr<ID3D11Device>& device, std::wstring shaderpath);
	ID3D11PixelShader* GetShader();
	ID3D10Blob* GetBuffer();
private:
	winrt::com_ptr<ID3D11PixelShader> shader = nullptr;
	winrt::com_ptr<ID3D10Blob> shader_buffer = nullptr;
};

