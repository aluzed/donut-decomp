// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <Render/Mesh.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace Donut
{
namespace GL
{
class ShaderProgram;
}

namespace P3D
{
class StaticEntity;
class InstancedStaticPhysics;
} // namespace P3D

class Entity
{
public:
	Entity() = default;
	virtual ~Entity() = default;

	virtual void Draw(GL::ShaderProgram&, bool opaque) {}
	// The box it fills in the world, to leave it out when unseen; none, it is
	// always drawn.
	virtual const BoundingBox* GetBounds() { return nullptr; }

	const std::string& GetName() const { return _name; }
	virtual const std::string GetClassName() const { return "Entity"; }

protected:
	std::string _name;
};

class StaticEntity: public Entity
{
public:
	StaticEntity(const P3D::StaticEntity&);

	void Draw(GL::ShaderProgram&, bool opaque) override;
	// Its mesh stands where it is in the world.
	const BoundingBox* GetBounds() override { return &_mesh->GetBounds(); }

	const std::string GetClassName() const override { return "StaticEntity"; }

protected:
	std::unique_ptr<Mesh> _mesh;
};

class InstancedStaticEntity: public Entity
{
public:
	InstancedStaticEntity(const P3D::Geometry&, const std::vector<Matrix4x4>&);

	void Draw(GL::ShaderProgram&, bool opaque) override;
	void SetInstanceTransform(std::size_t instance, const Matrix4x4& transform) { _mesh->SetTransform(instance, transform); }
	const BoundingBox* GetBounds() override { return &_mesh->GetInstancesBounds(); }

	const std::string GetClassName() const override { return "InstancedStaticEntity"; }

protected:
	std::unique_ptr<MeshInstanced> _mesh;
};

} // namespace Donut
