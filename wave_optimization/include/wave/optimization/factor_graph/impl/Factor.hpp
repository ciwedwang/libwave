#include <ceres/autodiff_cost_function.h>

namespace wave {

namespace internal {
// Template helpers used only in this file

/** Constructs a ValueView mapping the given raw array.
 *
 * @tparam V the type of FactorVariable
 * @tparam T the scalar type used by the optimizer
 * @param param the pointer provided by the optimizer
 * @return a FactorValue mapping the given array
 */
template <typename V, typename T>
inline typename V::ViewType make_value(T const *const param) {
    // The array is const, but ValueView maps non-const arrays. It would be
    // complicated to have two variants of ValueView, one for const and one
    // for non-const arrays. In this case, we know that the ValueView itself
    // is const, and it won't modify the array itself - thus, nothing will
    // modify the array, and it's "safe" to cast away the constness. Still,
    // @todo: reconsider this cast
    const auto ptr = const_cast<T *>(param);
    return typename V::ViewType{ptr};
}

}  // namespace internal

template <typename M, typename... V>
Factor<M, V...>::Factor(Factor::FuncType measurement_function,
                        M measurement,
                        std::shared_ptr<V>... variable_ptrs)
    : measurement_function{measurement_function},
      measurement{measurement},
      variable_ptrs{{variable_ptrs...}} {}

template <typename M, typename... V>
std::unique_ptr<ceres::CostFunction> Factor<M, V...>::costFunction() const
  noexcept {
    using Functor = FactorCostFunctor<M, V...>;
    using CostFunction =
      ceres::AutoDiffCostFunction<Functor, M::Size, V::Size...>;
    return std::unique_ptr<ceres::CostFunction>{
      new CostFunction{new Functor{this}}};
}

template <typename M, typename... V>
template <typename T>
bool Factor<M, V...>::evaluateRaw(tmp::replace<T, V> const *const... parameters,
                                  T *raw_residuals) const noexcept {
    auto residuals = internal::make_value<M>(raw_residuals);

    // Call the measurement function
    bool ok =
      measurement_function(internal::make_value<V>(parameters)..., residuals);
    if (ok) {
        // Calculate the normalized residual
        const auto &L = measurement.noise.inverseSqrtCov();
        residuals.asVector() =
          L * (residuals.asVector() - measurement.value.asVector());
    }
    return ok;
}

template <typename M, typename... V>
void Factor<M, V...>::print(std::ostream &os) const {
    os << "[";
    os << "Factor arity " << NumVars << ", ";
    os << "variables: ";
    for (auto i = 0; i < NumVars; ++i) {
        const auto &v = this->variable_ptrs[i];
        os << *v << "(" << v << ")";
        if (i < NumVars - 1) {
            os << ", ";
        }
    }
    os << "]";
}

}  // namespace wave
