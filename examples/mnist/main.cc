/*
 * main.cc
 * Copyright (C) 2019
 *
 * Distributed under terms of the MIT license.
 */

#include "mnist.hpp"
#include "ncg.h"
#include "nn/ops.h"

namespace ncg {

std::ostream &print_matrix(std::ostream &out, ncg::TensorPtr tensor) {
    ncg_assert(tensor->desc().dim() == 2);
    out << "[";
    for (ssize_t i = 0; i < tensor->desc().shape(0); ++i) {
        if (i != 0) out << std::endl << " ";
        out << "[";
        for (ssize_t j = 0; j < tensor->desc().shape(1); ++j) {
            if (j != 0) out << ", ";
            out << tensor->as<ncg::DTypeName::Float32>()->at(i, j);
        }
        out << "],";
    }

    return out;
}

} /* !namespace ncg */

namespace mnist {

ncg::TensorPtr ncg_read_mnist_image(std::string filename) {
    auto images = ncg::fromcc(ncg::DTypeName::Float32, mnist::read_mnist_image(filename));

    ncg::OpContext ctx;
    auto reshape_op = ncg::OpReshape();
    reshape_op.set_desc(ncg::OpDescPtr(new ncg::OpReshapeDesc({images->desc().shape(0), 1, 28, 28})));
    auto output_vec = reshape_op.execute(ctx, {images});
    ncg_assert_msg(!ctx.is_error(), ctx.error_str());
    return output_vec[0];
}

ncg::TensorPtr ncg_read_mnist_label(std::string filename) {
    return ncg::fromcc(ncg::DTypeName::Int32, mnist::read_mnist_label(filename));
}

void print_data(ncg::TensorPtr raw_images, ncg::TensorPtr raw_labels, ssize_t index) {
    auto images = raw_images->as<ncg::DTypeName::Float32>();
    auto labels = raw_labels->as<ncg::DTypeName::Int32>();

    std::cerr << "Image #" << index << " (Label: " << labels->at(index) << ")" << std::endl;
    for (ssize_t i = 0; i < 28; ++i) {
        for (ssize_t j = 0; j < 28; ++j) {
            if (j != 0) std::cerr << " ";
            std::cerr << ((images->at(index, ssize_t(0), i, j) > 0) ? 'X' : ' ');
        }
        std::cerr << std::endl;
    }
}

} /* !namespace mnist */

namespace mnist_model {

using namespace ncg;

struct MnistModel {
    MnistModel(std::mt19937 &rng) : rng(rng) {
        image = G::placeholder("image", {100, 784}, DTypeName::Float32);
        label = G::placeholder("label", {100}, DTypeName::Int64);
        linear1 = G::linear("linear1", image, 512, rng);
        activation1 = G::tanh(linear1);
        logits = G::linear("linear2", activation1, 10, rng);
        pred = logits.max(-1)[1];

        // prob = G::softmax(logits, -1);
        // loss = G::xent_sparse(prob, label, -1).mean(0);
    }

    // GTensorVec train_ops(double lr=0.01) {
    //     GTensorVec ops;
    //     auto &graph = get_default_graph();

    //     graph.backward(loss);
    //     for (const auto &name : {"linear1:W", "linear2:W", "linear1:b", "linear2:b"}) {
    //         auto W = graph.find_op(name)->outputs()[0];
    //         auto G = W->grad(loss);
    //         auto new_W = W - G * lr;
    //         ops.push_back(G::assign(W, new_W));
    //     }

    //     return ops;
    // }

    // TensorVec run(const GTensorVec &outputs, TensorPtr image, TensorPtr label=nullptr) {
    //     GraphForwardContext ctx;
    //     ctx.feed("image", image);

    //     if (label != nullptr) {
    //         ctx.feed("label", label);
    //     }

    //     return ctx.eval(outputs);
    // }

    std::mt19937 &rng;
    GTensorPtr image, label, linear1, activation1, logits, prob, pred, loss;
};

} /* !namespace mnist_model */

int main() {
    // auto train_images = mnist::ncg_read_mnist_image("./data/train-images-idx3-ubyte");
    // auto train_labels = mnist::ncg_read_mnist_label("./data/train-labels-idx1-ubyte");

    std::random_device rd{};
    std::mt19937 rng{rd()};

    auto test_images = mnist::ncg_read_mnist_image("./data/t10k-images-idx3-ubyte");
    auto test_labels = mnist::ncg_read_mnist_label("./data/t10k-labels-idx1-ubyte");

    // for (ssize_t i = 0; i < 10; ++i) {
    //     mnist::print_data(test_images, test_labels, i);
    // }

    auto model = std::make_unique<mnist_model::MnistModel>(rng);
    // std::cerr << model->logits << std::endl;
    // auto outputs = model->run({model->pred}, test_images.narrow(0, 0, 100).reshape({100, 784}));
    // std::cerr << outputs[0] << std::endl;

    return 0;
}

