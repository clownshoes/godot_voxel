#include "voxel_preload_completion_tracker.h"

namespace zylann::voxel {

Ref<VoxelPreloadCompletionTracker> VoxelPreloadCompletionTracker::create(std::shared_ptr<AsyncDependencyTracker> tracker) {
	Ref<VoxelPreloadCompletionTracker> self;
	self.instantiate();
	self->_tracker = tracker;
	self->_total_tasks = tracker->get_remaining_count();
	return self;
}

bool VoxelPreloadCompletionTracker::is_complete() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, false);
	return _tracker->is_complete();
}

bool VoxelPreloadCompletionTracker::is_aborted() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, false);
	return _tracker->is_aborted();
}

int VoxelPreloadCompletionTracker::get_total_tasks() const {
	return _total_tasks;
}

int VoxelPreloadCompletionTracker::get_remaining_tasks() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, 0);
	return _tracker->get_remaining_count();
}

void VoxelPreloadCompletionTracker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_complete"), &VoxelPreloadCompletionTracker::is_complete);
	ClassDB::bind_method(D_METHOD("is_aborted"), &VoxelPreloadCompletionTracker::is_aborted);
	ClassDB::bind_method(D_METHOD("get_total_tasks"), &VoxelPreloadCompletionTracker::get_total_tasks);
	ClassDB::bind_method(D_METHOD("get_remaining_tasks"), &VoxelPreloadCompletionTracker::get_remaining_tasks);
}

} // namespace zylann::voxel
