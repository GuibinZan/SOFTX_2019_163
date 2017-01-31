#pragma once

#include "../math.hpp"

namespace tomo {
namespace dim {

/** The iterator used for the Joseph DIM. */
template <typename T>
using joseph_iterator = typename std::vector<math::matrix_element<T>>::iterator;

/**
 * The Joseph DIM performs a single step along an axis, and the interpolates
 * between the other axes. A benefit of this technique is that there are no
 * 'shadowing non-zeros', i.e. non-zeros with the same indices.
 */
template <tomo::dimension D, typename T>
class joseph : public base<D, T, joseph_iterator<T>> {
  public:
    /** Construct the DIM for a given volume. */
    joseph(volume<D> vol) : base<D, T, joseph_iterator<T>>(vol) {
        auto dims = this->volume_.dimensions();
        auto max_width = tomo::math::max_element<D, int>(dims);
        queue_.reserve((int)(2 * max_width));
    }

  private:
    void reset_(math::line<D, T> line) override {
        // set the initial point
        auto current_point = line.origin;

        // first decide which action to take
        // the axis should correspond to the largest delta component, for
        // highest precision
        int axis = math::max_index<D, T>(math::abs(line.delta));

        auto step = line.delta / math::abs(line.delta[axis]);
        // this should get us on '0.5', biasing towards a 0.5 inside the volume
        auto nearest_column =
            math::round(current_point[axis] - (T)0.5) + (T)0.5;
        auto initial_step = (nearest_column - current_point[axis]) * step;
        if (step[axis] > 0) {
            current_point += initial_step;
        } else {
            current_point -= initial_step;
        }

        auto slice_volume_lengths =
            math::restrict<D, int>(this->volume_.lengths(), axis);
        auto slice_volume_origin =
            math::restrict<D, int>(this->volume_.origin(), axis);
        auto slice_volume =
            tomo::volume<D - 1>(slice_volume_origin, slice_volume_lengths);

        while (math::inside_margin<D, T>(current_point - step, this->volume_,
                                         (T)1.0)) {
            current_point -= step;
        }

        while (
            math::inside_margin<D, T>(current_point, this->volume_, (T)1.0)) {
            // we want to form the D-1 dimensional point, and the D-1
            // dimensional volume
            // we interpolate, and add the proper points (while knowing the
            // fixed axis coordinate)
            int current_row = math::round(current_point[axis] -
                                          this->volume_.origin()[axis] - 0.5);

            if (current_row >= this->volume_[axis] || current_row < 0) {
                current_point += step;
                continue;
            }

            auto initial_size = this->queue_.size();
            math::interpolate(math::restrict<D, T>(current_point, axis),
                              slice_volume, this->queue_);

            // add offsets to all points added
            // TODO we may want to change interpolate to not roll, so that we do
            // not have to unroll here
            for (auto i = initial_size; i < this->queue_.size(); ++i) {
                auto slice_index = slice_volume.unroll(this->queue_[i].index);
                auto extended_slice_index =
                    math::extend<D, int>(slice_index, axis, current_row);
                int voxel_index = this->volume_.index(extended_slice_index);
                this->queue_[i].index = voxel_index;
            }

            current_point += step;
        }

        this->line_ = line;
    }

    void clear_() override { queue_.clear(); }

    joseph_iterator<T> begin_() override { return queue_.begin(); }
    joseph_iterator<T> end_() override { return queue_.end(); }

    std::vector<math::matrix_element<T>> queue_;
};

} // namespace dim
} // namespace tomo
