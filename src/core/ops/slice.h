/*
 * slice.h
 * Copyright (C) 2019
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include "core/op.h"

namespace ncg {

class OpConcatDesc : public OpDesc {
public:
    OpConcatDesc() : axis(0) {}
    OpConcatDesc(ssize_t axis) : axis(axis) {}
    virtual ~OpConcatDesc() = default;

    ssize_t axis;
};

class OpConcat : public Op {
public:
    NCG_OP_DEF_NAME(OpConcat);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NONEMPTY_INPUTS(ctx, inputs);
        NCG_OP_CHECK_COMPATIBLE_DTYPE(ctx, inputs);
        NCG_OP_CHECK_COMPATIBLE_DIM(ctx, inputs);

        auto axis = this->template desc<OpConcatDesc>().axis;

        for (ssize_t i = 0; i < inputs.size(); ++i) {
            for (ssize_t j = 0; j < inputs[0]->desc().dim(); ++j) {
                if (j != axis) {
                    if (inputs[i]->desc().shape(j) != inputs[0]->desc().shape(j)) {
                        auto &err_stream = ctx.error(this) << "Concat op: inputs shape can only differ along the " << axis << " dimension; but got: ";
                        NCG_OP_CHECK_COMPATIBLE_SHAPE_PRINT(ctx, inputs, err_stream);
                        err_stream << ".";
                        return;
                    }
                }
            }
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        auto axis = this->template desc<OpConcatDesc>().axis;

        auto shape = inputs[0]->desc().shape_vec();
        for (ssize_t i = 1; i < inputs.size(); ++i) {
            shape[axis] += inputs[i]->desc().shape(axis);
        }

        auto output = empty(inputs[0]->desc().dtype(), shape);

#define CONCAT_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs, output);
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), CONCAT_DTYPE_CASE);
#undef CONCAT_DTYPE_CASE

        return {output};
    }

private:
    template <DTypeName DT>
    void kernel_(const TensorVec &inputs, TensorPtr output) {
        std::vector<const TensorImpl<DT> *> inputs_dtype;
        for (auto i : inputs) {
            inputs_dtype.emplace_back(i->as<DT>());
        }
        auto output_dtype = output->as<DT>();

        auto axis = this->template desc<OpConcatDesc>().axis;
        ssize_t index = 0;
        for (ssize_t i = 0; i < inputs.size(); ++i) {
            for (ssize_t j = 0; j < inputs[i]->desc().numel(); ++j) {
                ssize_t k = output->elindex(j) + index * output->desc().stride(axis);
                output_dtype->mutable_data_ptr()[k] = inputs_dtype[i]->elat(j);
            }
            index += inputs[i]->desc().shape(axis);
        }
    }
};

class OpSplitDesc : public OpDesc {
public:
    OpSplitDesc() : axis(0), splits() {}
    OpSplitDesc(ssize_t axis, const ShapeVec &splits) : axis(axis), splits(splits) {}
    virtual ~OpSplitDesc() = default;

    ssize_t axis;
    ShapeVec splits;
};

class OpSplit : public Op {
public:
    NCG_OP_DEF_NAME(OpSplit);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 1);

        auto &splits = this->template desc<OpSplitDesc>().splits;
        auto axis = this->template desc<OpSplitDesc>().axis;

        ssize_t nr_total = 0;
        for (ssize_t i = 0; i < splits.size(); ++i) {
            nr_total += splits[i];
        }

        if (nr_total != inputs[0]->desc().shape(axis)) {
            ctx.error(this) << "Split values are not consistent with the shape.";
            return;
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        const auto input = inputs[0];
        auto &splits = this->template desc<OpSplitDesc>().splits;
        auto axis = this->template desc<OpSplitDesc>().axis;
        TensorVec outputs(splits.size());

        ssize_t index = 0;
        for (ssize_t i = 0; i < splits.size(); ++i) {
            TensorDesc desc(input->desc().dtype(), input->desc().shape_vec(), input->desc().stride_vec());
            desc.shape(axis) = splits[i];
            ssize_t offset = index * desc.stride(axis);
            outputs[i] = tensor(desc, input->storage(), false, offset);
            index += splits[i];
        }

        return outputs;
    }
};

class OpNarrowDesc : public OpDesc {
public:
    OpNarrowDesc() : axis(0), start(0), length(0) {}
    OpNarrowDesc(ssize_t axis, ssize_t start, ssize_t length) : axis(axis), start(start), length(length) {}
    virtual ~OpNarrowDesc() = default;

    ssize_t axis, start, length;
};

class OpNarrow : public Op {
public:
    NCG_OP_DEF_NAME(OpNarrow);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 1);

        const auto &desc = this->template desc<OpNarrowDesc>();
        if (!(inputs[0]->desc().dim() >= desc.axis && inputs[0]->desc().shape(desc.axis) >= desc.start + desc.length)) {
            ctx.error(this) << "Invalid input range: start = " << desc.start << ", length = " << desc.length << ", input tensor size = " << inputs[0]->desc().shape(desc.axis) << ".";
            return;
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        const auto &desc = this->template desc<OpNarrowDesc>();

        auto input = inputs[0];
        input->make_contiguous();

        auto output_desc = input->desc();
        output_desc.shape(desc.axis) = desc.length;
        ssize_t output_data_ptr_offset = input->data_ptr_offset() + output_desc.stride(desc.axis) * desc.start;
        TensorPtr output = tensor(output_desc, input->storage(), false, output_data_ptr_offset);

        return {output};
    }
};

class OpNarrowBackwardDesc : public OpDesc {
public:
    OpNarrowBackwardDesc() : axis(0), start(0), input_size(0) {}
    OpNarrowBackwardDesc(ssize_t axis, ssize_t start, ssize_t input_size) : axis(axis), start(start), input_size(input_size) {}
    virtual ~OpNarrowBackwardDesc() = default;

    ssize_t axis, start, input_size;
};

class OpNarrowBackward : public Op {
public:
    NCG_OP_DEF_NAME(OpNarrowBackward);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 1);
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        const auto &desc = this->template desc<OpNarrowBackwardDesc>();

        auto shape = inputs[0]->desc().shape_vec();
        shape[desc.axis] = desc.input_size;
        auto output = zeros(inputs[0]->desc().dtype(), shape);

#define NARROWBACKWARD_DTYPE_CASE(dtype_name) kernel_<DTypeName::dtype_name>(inputs[0]->template as<DTypeName::dtype_name>(), output->template as<DTypeName::dtype_name>(), desc.axis, desc.start);
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), NARROWBACKWARD_DTYPE_CASE);
#undef NARROWBACKWARD_DTYPE_CASE

        return {output};
    }

private:
    template <DTypeName DT>
    void kernel_(const TensorImpl<DT> *input, TensorImpl<DT> *output, ssize_t axis, ssize_t start) {
        auto input_default_stride = input->desc().get_default_stride();
        auto output_default_stride = output->desc().get_default_stride();

        for (ssize_t i = 0; i < input->desc().numel(); ++i) {
            ssize_t j1, j2, j3;

            if (axis != 0) {
                j1 = i / input_default_stride[axis - 1];
                j2 = (i % input_default_stride[axis - 1]) / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            } else {
                j1 = 0;
                j2 = i / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            }

            ssize_t ii = j1 * ((axis == 0) ? 0 : output_default_stride[axis - 1]) + (j2 + start) * output_default_stride[axis] + j3;
            output->mutable_elat(ii) = input->elat(i);
        }
    }
};

class OpIndexSelectDesc : public OpDesc {
public:
    OpIndexSelectDesc() : axis(0) {}
    OpIndexSelectDesc(ssize_t axis) : axis(axis) {}
    virtual ~OpIndexSelectDesc() = default;

    ssize_t axis;
};

class OpIndexSelect : public Op {
public:
    NCG_OP_DEF_NAME(OpIndexSelect);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 2);
        NCG_OP_CHECK_INPUT_DTYPE_INT(ctx, inputs, 1);
        NCG_OP_CHECK_INPUT_DIM(ctx, inputs, 1, 1);

        const auto &desc = this->template desc<OpIndexSelectDesc>();
        if (!(0 <= desc.axis && desc.axis < inputs[0]->desc().dim())) {
            ctx.error(this) << "Invalid axis.";
            return;
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        auto axis = this->template desc<OpIndexSelectDesc>().axis;

        auto shape = inputs[0]->desc().shape_vec();
        shape[axis] = inputs[1]->desc().shape(0);
        auto output = empty(inputs[0]->desc().dtype(), shape);

        if (inputs[1]->desc().dtype() == DTypeName::Int32) {
#define INDEXSELECT32_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int32>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), INDEXSELECT32_DTYPE_CASE);
#undef INDEXSELECT32_DTYPE_CASE
        } else if (inputs[1]->desc().dtype() == DTypeName::Int64) {
#define INDEXSELECT64_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int64>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), INDEXSELECT64_DTYPE_CASE);
#undef INDEXSELECT64_DTYPE_CASE
        }

        return {output};
    }

private:
    template <DTypeName DT, DTypeName IndexDT>
    void kernel_(const TensorImpl<DT> *input, const TensorImpl<IndexDT> *index, TensorImpl<DT> *output) {
        auto axis = this->template desc<OpIndexSelectDesc>().axis;
        auto input_default_stride = input->desc().get_default_stride();
        auto output_default_stride = output->desc().get_default_stride();

        for (ssize_t i = 0; i < output->desc().numel(); ++i) {
            ssize_t j1, j2, j3;

            if (axis != 0) {
                j1 = i / output_default_stride[axis - 1];
                j2 = (i % output_default_stride[axis - 1]) / output_default_stride[axis];
                j3 = i % output_default_stride[axis];
            } else {
                j1 = 0;
                j2 = i / output_default_stride[axis];
                j3 = i % output_default_stride[axis];
            }

            ssize_t k = static_cast<ssize_t>(index->at(j2));
            ssize_t ii = j1 * ((axis == 0) ? 0 : input_default_stride[axis - 1]) + k * input_default_stride[axis] + j3;

            output->mutable_elat(i) = input->elat(ii);
        }
    }

};

class OpIndexSelectBackwardDesc : public OpDesc {
public:
    OpIndexSelectBackwardDesc() : axis(0) {}
    OpIndexSelectBackwardDesc(ssize_t axis, ssize_t input_size) : axis(axis), input_size(input_size) {}
    virtual ~OpIndexSelectBackwardDesc() = default;

    ssize_t axis, input_size;
};

class OpIndexSelectBackward : public Op {
public:
    NCG_OP_DEF_NAME(OpIndexSelectBackward);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 2);
        NCG_OP_CHECK_INPUT_DTYPE_INT(ctx, inputs, 1);
        NCG_OP_CHECK_INPUT_DIM(ctx, inputs, 1, 1);

        const auto &desc = this->template desc<OpIndexSelectBackwardDesc>();
        if (!(0 <= desc.axis && desc.axis < inputs[0]->desc().dim())) {
            ctx.error(this) << "Invalid axis.";
            return;
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        const auto &desc = this->template desc<OpIndexSelectBackwardDesc>();

        auto shape = inputs[0]->desc().shape_vec();
        shape[desc.axis] = desc.input_size;
        auto output = zeros(inputs[0]->desc().dtype(), shape);

        if (inputs[1]->desc().dtype() == DTypeName::Int32) {
#define INDEXSELECTBACKWARD32_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int32>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), INDEXSELECTBACKWARD32_DTYPE_CASE);
#undef INDEXSELECTBACKWARD32_DTYPE_CASE
        } else if (inputs[1]->desc().dtype() == DTypeName::Int64) {
#define INDEXSELECTBACKWARD64_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int64>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), INDEXSELECTBACKWARD64_DTYPE_CASE);
#undef INDEXSELECTBACKWARD64_DTYPE_CASE
        }

        return {output};
    }

private:
    template <DTypeName DT, DTypeName IndexDT>
    void kernel_(const TensorImpl<DT> *input, const TensorImpl<IndexDT> *index, TensorImpl<DT> *output) {
        auto axis = this->template desc<OpIndexSelectBackwardDesc>().axis;
        auto input_default_stride = input->desc().get_default_stride();
        auto output_default_stride = output->desc().get_default_stride();

        for (ssize_t i = 0; i < input->desc().numel(); ++i) {
            ssize_t j1, j2, j3;

            if (axis != 0) {
                j1 = i / input_default_stride[axis - 1];
                j2 = (i % input_default_stride[axis - 1]) / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            } else {
                j1 = 0;
                j2 = i / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            }

            ssize_t k = static_cast<ssize_t>(index->at(j2));
            ssize_t ii = j1 * ((axis == 0) ? 0 : output_default_stride[axis - 1]) + k * output_default_stride[axis] + j3;

            output->mutable_elat(ii) += input->elat(i);
        }
    }
};

class OpGatherDesc : public OpDesc {
public:
    OpGatherDesc() : axis(0) {}
    OpGatherDesc(ssize_t axis) : axis(axis) {}
    virtual ~OpGatherDesc() = default;

    ssize_t axis;
};

class OpGather : public Op {
public:
    NCG_OP_DEF_NAME(OpGather);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 2);
        NCG_OP_CHECK_INPUT_DTYPE_INT(ctx, inputs, 1);
        NCG_OP_CHECK_COMPATIBLE_DIM(ctx, inputs);

        const auto &desc = this->template desc<OpGatherDesc>();
        if (!(0 <= desc.axis && desc.axis < inputs[0]->desc().dim())) {
            ctx.error(this) << "Invalid axis.";
            return;
        }

        for (ssize_t i = 0; i < inputs[0]->desc().dim(); ++i) {
            if (i == desc.axis) continue;
            if (inputs[0]->desc().shape(i) != inputs[1]->desc().shape(i)) {
                ctx.error(this) << "The inputs should have the same shape except the demanding axis.";
                return;
            }
        }
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        auto axis = this->template desc<OpGatherDesc>().axis;

        auto output = empty(inputs[0]->desc().dtype(), inputs[1]->desc().shape_vec());

        if (inputs[1]->desc().dtype() == DTypeName::Int32) {
#define GATHER32_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int32>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), GATHER32_DTYPE_CASE);
#undef GATHER32_DTYPE_CASE
        } else if (inputs[1]->desc().dtype() == DTypeName::Int64) {
#define GATHER64_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int64>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), GATHER64_DTYPE_CASE);
#undef GATHER64_DTYPE_CASE
        }

        return {output};
    }

private:
    template <DTypeName DT, DTypeName IndexDT>
    void kernel_(const TensorImpl<DT> *input, const TensorImpl<IndexDT> *index, TensorImpl<DT> *output) {
        auto axis = this->template desc<OpGatherDesc>().axis;
        auto input_default_stride = input->desc().get_default_stride();
        auto output_default_stride = output->desc().get_default_stride();

        for (ssize_t i = 0; i < output->desc().numel(); ++i) {
            ssize_t j1, j2, j3;

            if (axis != 0) {
                j1 = i / output_default_stride[axis - 1];
                j2 = (i % output_default_stride[axis - 1]) / output_default_stride[axis];
                j3 = i % output_default_stride[axis];
            } else {
                j1 = 0;
                j2 = i / output_default_stride[axis];
                j3 = i % output_default_stride[axis];
            }

            ssize_t k = static_cast<ssize_t>(index->elat(i));
            ssize_t ii = j1 * ((axis == 0) ? 0 : input_default_stride[axis - 1]) + k * input_default_stride[axis] + j3;

            output->mutable_elat(i) = input->elat(ii);
        }
    }
};

class OpGatherBackwardDesc : public OpDesc {
public:
    OpGatherBackwardDesc() : axis(0) {}
    OpGatherBackwardDesc(ssize_t axis, ssize_t input_size) : axis(axis), input_size(input_size) {}
    virtual ~OpGatherBackwardDesc() = default;

    ssize_t axis, input_size;
};

class OpGatherBackward : public Op {
public:
    NCG_OP_DEF_NAME(OpGatherBackward);

    virtual void check_inputs(OpContext &ctx, const TensorVec &inputs) {
        NCG_OP_CHECK_NR_INPUTS(ctx, inputs, 2);
        NCG_OP_CHECK_INPUT_DTYPE_INT(ctx, inputs, 1);
        NCG_OP_CHECK_COMPATIBLE_DIM(ctx, inputs);
    }

    virtual TensorVec compute(OpContext &ctx, const TensorVec &inputs) {
        const auto &desc = this->template desc<OpGatherBackwardDesc>();

        auto shape = inputs[0]->desc().shape_vec();
        shape[desc.axis] = desc.input_size;
        auto output = zeros(inputs[0]->desc().dtype(), shape);

        if (inputs[1]->desc().dtype() == DTypeName::Int32) {
#define GATHERBACKWARD32_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int32>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), GATHERBACKWARD32_DTYPE_CASE);
#undef GATHERBACKWARD32_DTYPE_CASE
        } else if (inputs[1]->desc().dtype() == DTypeName::Int64) {
#define GATHERBACKWARD64_DTYPE_CASE(dtype) kernel_<DTypeName::dtype>(inputs[0]->template as<DTypeName::dtype>(), inputs[1]->template as<DTypeName::Int64>(), output->template as<DTypeName::dtype>());
NCG_DTYPE_SWITCH_ALL(inputs[0]->desc().dtype(), GATHERBACKWARD64_DTYPE_CASE);
#undef GATHERBACKWARD64_DTYPE_CASE
        }

        return {output};
    }

private:
    template <DTypeName DT, DTypeName IndexDT>
    void kernel_(const TensorImpl<DT> *input, const TensorImpl<IndexDT> *index, TensorImpl<DT> *output) {
        auto axis = this->template desc<OpGatherBackwardDesc>().axis;
        auto input_default_stride = input->desc().get_default_stride();
        auto output_default_stride = output->desc().get_default_stride();

        for (ssize_t i = 0; i < input->desc().numel(); ++i) {
            ssize_t j1, j2, j3;

            if (axis != 0) {
                j1 = i / input_default_stride[axis - 1];
                j2 = (i % input_default_stride[axis - 1]) / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            } else {
                j1 = 0;
                j2 = i / input_default_stride[axis];
                j3 = i % input_default_stride[axis];
            }

            ssize_t k = static_cast<ssize_t>(index->elat(i));
            ssize_t ii = j1 * ((axis == 0) ? 0 : output_default_stride[axis - 1]) + k * output_default_stride[axis] + j3;

            output->mutable_elat(ii) += input->elat(i);
        }
    }
};

} /* !namespace ncg */

