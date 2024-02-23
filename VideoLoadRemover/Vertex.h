#pragma once
#include <DirectXMath.h>

struct Vertex {
	Vertex() {};
	Vertex(float x, float y, float r, float g, float b, float a)
		:pos(x, y), color(r,g,b,a) {}
	::DirectX::XMFLOAT2 pos;
	::DirectX::XMFLOAT4 color;

};