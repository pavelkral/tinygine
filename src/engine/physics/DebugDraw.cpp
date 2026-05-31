#include "engine/EngineDependencies.h"
#include "engine/physics/DebugDraw.h"

std::vector<Vertex>* DebugDraw::s_vertices = nullptr;

void DebugDraw::Begin(std::vector<Vertex>& vertices) { s_vertices = &vertices; }


void DebugDraw::DrawLine(const SM::Vector3& p1, const SM::Vector3& p2, const SM::Vector3& color) {
	if (!s_vertices) return; Vertex v1, v2; v1.pos = p1; v1.normal = color; v1.uv = { 0,0 }; v2.pos = p2; v2.normal = color; v2.uv = { 0,0 };
	s_vertices->push_back(v1); s_vertices->push_back(v2);
}


void DebugDraw::DrawBox(const SM::Vector3& pos, const SM::Quaternion& rot, const SM::Vector3& extents, const SM::Vector3& color) {
	SM::Matrix m = SM::Matrix::CreateScale(extents) * SM::Matrix::CreateFromQuaternion(rot) * SM::Matrix::CreateTranslation(pos);
	SM::Vector3 pts[8] = { {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1}, {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1} };
	for (int i = 0; i < 8; ++i) pts[i] = SM::Vector3::Transform(pts[i], m);
	DrawLine(pts[0], pts[1], color); DrawLine(pts[1], pts[2], color); DrawLine(pts[2], pts[3], color); DrawLine(pts[3], pts[0], color);
	DrawLine(pts[4], pts[5], color); DrawLine(pts[5], pts[6], color); DrawLine(pts[6], pts[7], color); DrawLine(pts[7], pts[4], color);
	DrawLine(pts[0], pts[4], color); DrawLine(pts[1], pts[5], color); DrawLine(pts[2], pts[6], color); DrawLine(pts[3], pts[7], color);
}


void DebugDraw::DrawCircle(const SM::Vector3& pos, const SM::Quaternion& rot, float radius, int segments, const SM::Vector3& color) {
	SM::Matrix m = SM::Matrix::CreateFromQuaternion(rot) * SM::Matrix::CreateTranslation(pos); SM::Vector3 lastPt;
	for (int i = 0; i <= segments; ++i) {
		float angle = (i / (float)segments) * XM_2PI; SM::Vector3 pt = SM::Vector3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
		pt = SM::Vector3::Transform(pt, m); if (i > 0) DrawLine(lastPt, pt, color); lastPt = pt;
	}
}


void DebugDraw::DrawSphere(const SM::Vector3& pos, const SM::Quaternion& rot, float radius, const SM::Vector3& color) {
	int segs = 16; DrawCircle(pos, rot, radius, segs, color);
	DrawCircle(pos, rot * SM::Quaternion::CreateFromAxisAngle(SM::Vector3::UnitX, XM_PIDIV2), radius, segs, color);
	DrawCircle(pos, rot * SM::Quaternion::CreateFromAxisAngle(SM::Vector3::UnitZ, XM_PIDIV2), radius, segs, color);
}


void DebugDraw::DrawCapsule(const SM::Vector3& pos, const SM::Quaternion& rot, float halfHeight, float radius, const SM::Vector3& color) {
	SM::Matrix m = SM::Matrix::CreateFromQuaternion(rot) * SM::Matrix::CreateTranslation(pos);
	SM::Vector3 topPos = SM::Vector3::Transform(SM::Vector3(0, halfHeight, 0), m); SM::Vector3 botPos = SM::Vector3::Transform(SM::Vector3(0, -halfHeight, 0), m);
	DrawCircle(topPos, rot, radius, 16, color); DrawCircle(botPos, rot, radius, 16, color);
	SM::Vector3 offsets[4] = { {radius, 0, 0}, {-radius, 0, 0}, {0, 0, radius}, {0, 0, -radius} };
	for (int i = 0; i < 4; ++i) {
		SM::Vector3 topOffset = SM::Vector3::Transform(offsets[i] + SM::Vector3(0, halfHeight, 0), m);
		SM::Vector3 botOffset = SM::Vector3::Transform(offsets[i] + SM::Vector3(0, -halfHeight, 0), m); DrawLine(topOffset, botOffset, color);
	}
}
