#include "expression.h"
#include <cmath>
#include <exception>

namespace et{

bool is_scalar(const var& v){ return v.getValue().size() == 1; }
double sval(const var& v){ return v.getValue()(0,0); }
MatrixXd mval(const var& v){ return v.getValue(); }

// Helper function for recursive propagation
MatrixXd _eval(op_type op, const std::vector<var>& operands){
    switch(op){
        case op_type::plus:{
            if(is_scalar(operands[0]))
                return sval(operands[0]) + mval(operands[1]).array();
            else if(is_scalar(operands[1]))
                return mval(operands[0]).array() + sval(operands[1]);
            else
                return mval(operands[0]).array() + mval(operands[1]).array();
        }
        case op_type::minus:{
            if(is_scalar(operands[0]))
                return sval(operands[0]) - mval(operands[1]).array();
            else if(is_scalar(operands[1]))
                return mval(operands[0]).array() - sval(operands[1]);
            else
                return mval(operands[0]).array() - mval(operands[1]).array();
        }
        case op_type::multiply:{
            if(is_scalar(operands[0]))
                return sval(operands[0]) * mval(operands[1]).array();
            else if(is_scalar(operands[1]))
                return mval(operands[0]).array() * sval(operands[1]);
            else
                return mval(operands[0]).array() * mval(operands[1]).array();
        }
        case op_type::divide:{
            if(is_scalar(operands[0]))
                return sval(operands[0]) / mval(operands[1]).array();
            else if(is_scalar(operands[1]))
                return mval(operands[0]).array() / sval(operands[1]);
            else
                return mval(operands[0]).array() / mval(operands[1]).array();
        }
        case op_type::exponent:{
            return mval(operands[0]).array().exp();
        }
        case op_type::log:{
            return mval(operands[0]).array().log();
        }
        case op_type::polynomial:{
            return mval(operands[0]).array().pow(sval(operands[1]));
        }
        case op_type::dot:{
            return mval(operands[0]) * mval(operands[1]);
        }
        case op_type::inverse:{
            return mval(operands[0]).inverse();
        }
        case op_type::transpose:{
            return mval(operands[0]).transpose();
        }
        case op_type::sum:{
            return scalar(mval(operands[0]).array().sum());
        }
        case op_type::none:{
            throw std::invalid_argument("Cannot have a non-leaf contain none-op.");
        }
    };
}

// Helper function for recursive backpropagation
MatrixXd _back_single(op_type op,
        const MatrixXd& dx,
        const std::vector<var>& operands,
        int op_idx){
    switch(op){
        case op_type::plus: {
            if(!is_scalar(operands[op_idx]))
                return dx;
            else
                return scalar(dx.array().sum());
        }
        case op_type::minus: {
            MatrixXd res = is_scalar(operands[op_idx]) ? scalar(dx.array().sum()) : dx;
            if(op_idx == 0)
                return res;
            else
                return -1 * res.array();
        }
        case op_type::multiply: {
            if(is_scalar(operands[op_idx]))
                return scalar((dx.array() * mval(operands[1-op_idx]).array()).sum());
            else if(is_scalar(operands[1-op_idx]))
                return dx.array() * sval(operands[1-op_idx]);
            else
                return dx.array() *
                    mval(operands[(1-op_idx)]).array();
        }
        case op_type::divide: {
            if(op_idx == 0){
                if(is_scalar(operands[0])) // the scalar
                    return scalar((dx.array() * (1 / mval(operands[1]).array())).sum());
                else if(is_scalar(operands[1]))
                    return dx.array() * (1 / sval(operands[1]));
                else
                    return dx.array() *
                        (1 / mval(operands[1]).array());
            }
            else{
                if(is_scalar(operands[1]))
                    return scalar((dx.array() *
                            (-mval(operands[0]).array() /
                             std::pow(sval(operands[1]),2))).sum());
                else if(is_scalar(operands[0]))
                    return dx.array() * (-sval(operands[0]) /
                        mval(operands[1]).array().pow(2));
                else
                    return dx.array() *
                        (-mval(operands[0]).array() /
                         mval(operands[1]).array().pow(2));
            }
        }
        case op_type::exponent: {
            return dx.array() *
                mval(operands[0]).array().exp();

        }
        case op_type::log: {
            return dx.array() *
                (1 / mval(operands[0]).array());
        }
        case op_type::polynomial: {
            if(op_idx == 0)
                return dx.array() *
                    mval(operands[0]).array().pow(sval(operands[1])-1) *
                        sval(operands[1]);
            else
                return scalar(0); // we don't support exponents other than e.
        }
        case op_type::dot: {
            if(op_idx == 0)
                return dx * mval(operands[1]).transpose();
            else
                return mval(operands[0]).transpose() * dx;
        }
        case op_type::inverse: {
            // (I)' = (AA^{-1})' = A(A^{-1}') + A'(A^{-1})
            // AA^{-1}' = -A'A^{-1}
            // A^{-1}AA^{-1}' = -A^{-1}A'A^{-1}
            // A^{-1}' = -A^{-1}A'A^{-1}
            // This means all of the next few chain rules are _nested_.
            // That makes this gradient way too hard. It doesn't work with our current
            // framework.
            throw std::invalid_argument("The derivative of an inverse is too hard.");
        }
        case op_type::transpose: {
            return dx.transpose();
        }
        case op_type::sum: {
            return sval(dx) * ones_like(operands[0]).array();
        }
        case op_type::none: {
            throw std::invalid_argument("Cannot have a non-leaf contain none-op.");
        }
    };
}

std::vector<MatrixXd> _back(op_type op, const std::vector<var>& operands,
        const std::unordered_set<var>& nonconsts,
        const MatrixXd& dx){
    std::vector<MatrixXd> derivatives;
    for(size_t i = 0; i < operands.size(); i++){
        if(nonconsts.find(operands[i]) == nonconsts.end())
            derivatives.push_back(zeros_like(operands[i])); // no gradient flow.
        else
            derivatives.push_back(_back_single(op, dx, operands, i));
    }
    return derivatives;
}

std::vector<MatrixXd> _back(op_type op, const std::vector<var>& operands,
        const MatrixXd& dx){
    std::vector<MatrixXd> derivatives;
    for(size_t i = 0; i < operands.size(); i++){
        derivatives.push_back(_back_single(op, dx, operands, i));
    }
    return derivatives;
}

expression::expression(var _root) : root(_root){}

var expression::getRoot() const{
    return root;
}

std::vector<var> expression::findLeaves(){
    std::unordered_set<var> leaves;
    std::queue<var> q;
    q.push(root);

    while(!q.empty()){
        var v = q.front();
        if(v.getChildren().empty()){
            leaves.insert(v);
        }
        else{
            std::vector<var> children = v.getChildren();
            for(const var& v : children)
                q.push(v);
        }
        q.pop();
    }
    std::vector<var> ans;
    std::copy(leaves.begin(), leaves.end(), std::back_inserter(ans));
    return ans;
}

void _rpropagate(var& v){
    if(v.getChildren().empty())
        return;
    std::vector<var> children = v.getChildren();
    for(var& _v : children){
        _rpropagate(_v);
    }
    v.setValue(_eval(v.getOp(), v.getChildren()));
}

MatrixXd expression::propagate(){
    _rpropagate(root);
    return root.getValue();
}

// This one is a little tougher:
// We want to create a queue for which can be parallelized;
// A node is "available" if it has all of its children loaded.
// Currently, we wrap around the vars, and add a field
// that tracks how many occurences of its children we found.
//
// Pseudocode:
// suppose q has all the leaves, and m contains <var, int>
// while q not empty:
//     v = q.pop
//     evaluate v
//     for parent in v.parents:
//         m[parent]++
//         if m[parent] has sufficient nums:
//             q.add(parent)
//
// return root.val

MatrixXd expression::propagate(const std::vector<var>& leaves){
    std::queue<var> q;
    std::unordered_map<var, int> explored;
    for(const var& v : leaves){
        q.push(v);
    }
    while(!q.empty()){
        var v = q.front();
        q.pop();
        std::vector<var> parents = v.getParents();
        for(var& parent : parents){
            explored[parent]++;
            if(numOpArgs(parent.getOp()) == explored[parent]){
                parent.setValue(_eval(parent.getOp(), parent.getChildren()));
                q.push(parent);
            }
        }
    }
    return root.getValue();
}

std::unordered_set<var> expression::findNonConsts(const std::vector<var>& leaves){
    std::unordered_set<var> nonconsts;
    std::queue<var> q;
    for(const var& v : leaves)
        q.push(v);

    while(!q.empty()){
        var v = q.front();
        q.pop();

        // We should not traverse this if it has already been visited.
        if(nonconsts.find(v) != nonconsts.end())
            continue;

        nonconsts.insert(v);
        std::vector<var> parents = v.getParents();
        for(const var& parent : parents){
            q.push(parent);
        }
    }
    return nonconsts;
}

// When we perform a BFS to find the derivatives, we should be careful not to
// 1. override its original value.
//     x = a + b
//     y = a + c
//     derivative of a is dx/da + dy/da
// 2. explore too much of the tree that is not unnecessary.
//     Ideally, our user would be smart and input a value directly rather than
//     create an entire expression subtree for a value that is a constant.
//     TODO: In the future, add a field in var that states whether it's a constant.

void expression::backpropagate(std::unordered_map<var, MatrixXd>& leaves){
    std::queue<var> q;
    std::unordered_map<var, MatrixXd> derivatives;
    std::unordered_map<var, int> explored;
    q.push(root);
    derivatives[root] = ones_like(root);

    while(!q.empty()){
        var v = q.front();
        q.pop();
        std::vector<var>& children = v.getChildren();
        std::vector<MatrixXd> child_derivs = _back(v.getOp(), children, derivatives[v]);
        for(size_t i = 0; i < children.size(); i++){
            auto child = children[i];
            if(explored.find(child) == explored.end())
                explored[child] = child.getParents().size();
            explored[child]--;
            // Be careful to not override the derivative value!
            if(derivatives.find(child) == derivatives.end())
                derivatives.emplace(child, zeros_like(child));
            derivatives[child] = derivatives[child].array() + child_derivs[i].array();
            if(children[i].getOp() != op_type::none && explored[child] == 0)
                q.push(child);
        }
    }

    // After we have retrieved the derivatives,
    // select the leaves and update in leaves.
    for(auto& iter : leaves){
        iter.second = derivatives[iter.first];
    }
}

// Restricted BFS: same as previous, but we will have a set of nonconsts to tell us
// where we can BFS to.
void expression::backpropagate(std::unordered_map<var, MatrixXd>& leaves,
        const std::unordered_set<var>& nonconsts){
    std::queue<var> q;
    std::unordered_map<var, MatrixXd> derivatives;
    std::unordered_map<var, int> explored;
    q.push(root);
    derivatives[root] = ones_like(root);

    while(!q.empty()){
        var v = q.front();
        q.pop();
        if(nonconsts.find(v) == nonconsts.end())
            continue;
        std::vector<var>& children = v.getChildren();
        std::vector<MatrixXd> child_derivs = _back(v.getOp(), children, nonconsts, derivatives[v]);
        for(size_t i = 0; i < children.size(); i++){
            auto child = children[i];
            if(explored.find(child) == explored.end())
                explored[child] = child.getParents().size();
            explored[child]--;
            // Be careful to not override the derivative value!
            if(derivatives.find(child) == derivatives.end())
                derivatives.emplace(child, zeros_like(child));
            derivatives[child] = derivatives[child].array() + child_derivs[i].array();
            if(children[i].getOp() != op_type::none && explored[child] == 0)
                q.push(child);
        }
    }

    // After we have retrieved the derivatives,
    // select the leaves and update in leaves.
    for(auto& iter : leaves){
        iter.second = derivatives[iter.first];
    }
}

}
