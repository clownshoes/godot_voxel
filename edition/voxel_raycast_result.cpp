#include "voxel_raycast_result.h"

namespace zylann::voxel {

Vector3i VoxelRaycastResult::_b_get_position() const {
	return position;
}

Vector3i VoxelRaycastResult::_b_get_previous_position() const {
	return previous_position;
}

float VoxelRaycastResult::_b_get_distance() const {
	return distance_along_ray;
}

Vector3 VoxelRaycastResult::_b_get_normal() const {
	return normal;
}

void VoxelRaycastResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &VoxelRaycastResult::_b_get_position);
	ClassDB::bind_method(D_METHOD("get_previous_position"), &VoxelRaycastResult::_b_get_previous_position);
	ClassDB::bind_method(D_METHOD("get_distance"), &VoxelRaycastResult::_b_get_distance);
	ClassDB::bind_method(D_METHOD("get_normal"), &VoxelRaycastResult::_b_get_normal);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "position"), "", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "previous_position"), "", "get_previous_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance"), "", "get_distance");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "normal"), "", "get_normal");
}

} // namespace zylann::voxel
