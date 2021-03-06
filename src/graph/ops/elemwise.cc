/*
 * elemwise.cc
 * Copyright (C) 2019
 *
 * Distributed under terms of the MIT license.
 */

#include "graph/ops/elemwise.h"
#include "graph/ops/netsrc.h"
#include "graph/ops/shape.h"

namespace ncg {

void GOpCast::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpCast>(OpDescPtr(new OpCastDesc(m_inputs[0]->desc().dtype())), output_grad)
    );
}

void GOpCond::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    auto zero_grad = graph.op<GOpZeros>(
        OpDescPtr(new OpZerosDesc(output_grad->desc().dtype(), output_grad->desc().shape_vec())),
        graph.op<GOpShapeOf>(nullptr, output_grad)
    );

    m_inputs[0]->set_grad(graph, loss, nullptr);
    m_inputs[1]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, m_inputs[0], output_grad, zero_grad));
    m_inputs[2]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, m_inputs[0], zero_grad, output_grad));
}

void GOpNeg::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpNeg>(nullptr, output_grad)
    );
}

void GOpSin::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpCos>(nullptr, m_inputs[0])
        )
    );
}

void GOpCos::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpNeg>(nullptr,
                graph.op<GOpSin>(nullptr, m_inputs[0])
            )
        )
    );
}

void GOpTan::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    auto cosx = graph.op<GOpCos>(nullptr, m_inputs[0]);
    auto secx = graph.op<GOpReciprocal>(nullptr, cosx);

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpMul>(nullptr, secx, secx)
        )
    );
}

void GOpLog::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpReciprocal>(nullptr, m_inputs[0])
        )
    );
}

void GOpExp::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            m_outputs[0]
        )
    );
}

void GOpTanh::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpSub>(nullptr,
                graph.op<GOpOnes>(
                    OpDescPtr(new OpOnesDesc(m_inputs[0]->desc().dtype(), m_inputs[0]->desc().shape_vec())),
                    graph.op<GOpShapeOf>(nullptr, m_inputs[0])
                ),
                graph.op<GOpMul>(nullptr, m_outputs[0], m_outputs[0])
            )
        )
    );
}

void GOpSigmoid::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad,
            graph.op<GOpMul>(nullptr,
                m_outputs[0],
                graph.op<GOpSub>(nullptr,
                    graph.op<GOpOnes>(
                        OpDescPtr(new OpOnesDesc(m_inputs[0]->desc().dtype(), m_inputs[0]->desc().shape_vec())),
                        graph.op<GOpShapeOf>(nullptr, m_inputs[0])
                    ),
                    m_outputs[0]
                )
            )
        )
    );
}

void GOpReciprocal::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpNeg>(nullptr,
            graph.op<GOpDiv>(nullptr,
                nullptr, output_grad,
                graph.op<GOpMul>(nullptr, m_inputs[0], m_inputs[0])
            )
        )
    );
}

void GOpAdd::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        output_grad
    );
    m_inputs[1]->set_grad(graph, loss,
        output_grad
    );
}

void GOpSub::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        output_grad
    );
    m_inputs[1]->set_grad(graph, loss,
        graph.op<GOpNeg>(nullptr, output_grad)
    );
}

void GOpMul::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad, m_inputs[1])
    );
    m_inputs[1]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr, output_grad, m_inputs[0])
    );
}

void GOpDiv::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpDiv>(nullptr, output_grad, m_inputs[1])
    );
    m_inputs[1]->set_grad(graph, loss,
        graph.op<GOpNeg>(nullptr,
            graph.op<GOpDiv>(nullptr,
                graph.op<GOpMul>(nullptr, output_grad, m_inputs[0]),
                graph.op<GOpMul>(nullptr, m_inputs[1], m_inputs[1])
            )
        )
    );
}

NCG_GOP_DEF_NO_GRAD(GOpGe);
NCG_GOP_DEF_NO_GRAD(GOpLe);
NCG_GOP_DEF_NO_GRAD(GOpGeq);
NCG_GOP_DEF_NO_GRAD(GOpLeq);
NCG_GOP_DEF_NO_GRAD(GOpEq);
NCG_GOP_DEF_NO_GRAD(GOpNeq);

void GOpPow::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    m_inputs[0]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr,
            output_grad,
            graph.op<GOpMul>(nullptr,
                m_inputs[1],
                graph.op<GOpPow>(nullptr,
                    m_inputs[0],
                    graph.op<GOpSub>(nullptr,
                        m_inputs[1],
                        graph.op<GOpOnes>(
                            OpDescPtr(new OpOnesDesc(
                                m_inputs[1]->desc().dtype(), m_inputs[1]->desc().shape_vec()
                            )),
                            graph.op<GOpShapeOf>(nullptr, m_inputs[1])
                        )
                    )
                )
            )
        )
    );
    m_inputs[1]->set_grad(graph, loss,
        graph.op<GOpMul>(nullptr,
            output_grad,
            graph.op<GOpMul>(nullptr,
                m_outputs[0],
                graph.op<GOpLog>(nullptr, m_inputs[0])
            )
        )
    );
}

void GOpMin::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    auto cond = graph.op<GOpLeq>(nullptr, m_inputs[0], m_inputs[1]);
    auto zero_grad = graph.op<GOpZeros>(
        OpDescPtr(new OpZerosDesc(output_grad->desc().dtype(), output_grad->desc().shape_vec())),
        graph.op<GOpShapeOf>(nullptr, output_grad)
    );
    m_inputs[0]->set_grad(graph, loss, nullptr);
    m_inputs[1]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, cond, output_grad, zero_grad));
    m_inputs[2]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, cond, zero_grad, output_grad));
}

void GOpMax::backward(Graph &graph, GTensorPtr loss) {
    auto output_grad = m_outputs[0]->grad(loss);
    if (output_grad == nullptr) {
        m_inputs[0]->set_grad(graph, loss, nullptr);
        m_inputs[1]->set_grad(graph, loss, nullptr);
        return;
    }

    auto cond = graph.op<GOpGeq>(nullptr, m_inputs[0], m_inputs[1]);
    auto zero_grad = graph.op<GOpZeros>(
        OpDescPtr(new OpZerosDesc(output_grad->desc().dtype(), output_grad->desc().shape_vec())),
        graph.op<GOpShapeOf>(nullptr, output_grad)
    );
    m_inputs[0]->set_grad(graph, loss, nullptr);
    m_inputs[1]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, cond, output_grad, zero_grad));
    m_inputs[2]->set_grad(graph, loss, graph.op<GOpCond>(nullptr, cond, zero_grad, output_grad));
}

} /* !namespace ncg */
