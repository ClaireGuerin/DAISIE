// [[Rcpp::plugins(cpp14)]]
// [[Rcpp::depends(BH)]]


//' @export daisie_odeint_cs


#include "DAISIE_odeint.h"


namespace {


  // maximal number of steps the solver is executing.
  // prevents odeint from getting stuck but is a horrible
  // at-hoc - 'solution'.
  static constexpr int max_steps = 1'000'000;


  //
  class padded_vector_view
  {
  public:
    padded_vector_view(int pad, const double* data, int n) :
    data_(data), pad_(pad), n_(n)
    {
    }

    // return 0.0 for indices 'i' outside [pad, pad + n)
    double operator[](int i) const
    {
      const auto ii = i - pad_;
      return (ii >= 0 && i < n_) ? *(data_ + ii) : 0.0;
    }

  private:
    const double* data_ = nullptr;  // this->operator[pad_] == *data_
    int pad_ = 0;
    int n_ = 0;
  };


  // common parameter
  struct param_t
  {
    param_t(int LX, int KK, state_type&& par) :
      lx(LX), kk(KK), P(std::move(par))
    {
    }

    int lx;
    int kk;
    state_type P;
    mutable int steps = 0;
  };


  class cpp_daisie_cs_runmod
  {
  public:
    cpp_daisie_cs_runmod(param_t&& par) :
      p_(std::move(par))
    {
    }

    // odeint interface
    void operator()(const state_type& x, state_type& dx, double) const
    {
      if (++p_.steps > max_steps) throw std::runtime_error("cpp_daisie_cs_runmod: too many steps");

      // xx1 = c(0,0,x[1:lx],0)
      // xx2 = c(0,0,x[(lx + 1):(2 * lx)],0)
      // xx3 = x[2 * lx + 1]
      // using padded views instead of vectors:
      const auto xx1 = padded_vector_view(2, x.data(), p_.lx);
      const auto xx2 = padded_vector_view(2, x.data() + p_.lx, p_.lx);
      const auto xx3 = x[2 * p_.lx];

      // DO I = 1, N + 4 + 2 * kk
      //   laavec(I) = P(I)
      //   lacvec(I) = P(I + N + 4 + 2 * kk)
      //   muvec(I)  = P(I + 2 * (N + 4 + 2 * kk))
      //   gamvec(I) = P(I + 3 * (N + 4 + 2 * kk))
      //   nn(I)     = P(I + 4 * (N + 4 + 2 * kk))
      // ENDDO
      // using views instead of vectors:
      const auto chunk = p_.lx + 4 + 2 * p_.kk;
      const auto laavec = p_.P.data();
      const auto lacvec = p_.P.data() + chunk;
      const auto muvec = p_.P.data() + 2 * chunk;
      const auto gamvec = p_.P.data() + 3 * chunk;
      const auto nn = p_.P.data() + 4 * chunk;

      // DO I = 3, N + 2
      //   il1(I - 2) = I + kk - 1
      //   il2(I - 2) = I + kk + 1
      //   il3in3(I - 2) = I + kk
      //   il4(I - 2) = I + kk - 2
      //   in1(I - 2) = I + 2 * kk - 1
      //   in2ix2(I - 2) = I + 1
      //   ix1(I - 2) = I - 1
      //   ix3(I - 2) = I
      //   ix4(I - 2) = I - 2
      // ENDDO
      // using offsets into our views instead of vectors:
      const int il1 = 2 + p_.kk - 1;
      const int il2 = 2 + p_.kk + 1;
      const int il3in3 = 2 + p_.kk;
      const int il4 = 2 + p_.kk - 2;
      const int in1 = 2 + 2 * p_.kk - 1;
      const int in2ix2 = 2 + 1;
      const int ix1 = 2 - 1;
      const int ix3 = 2;
      const int ix4 = 2 - 2;

      // DO I = 1, N
      //   FF1 = laavec(il1(I) + 1) * xx2(ix1(I))
      //   FF1 = FF1 + lacvec(il4(I) + 1) * xx2(ix4(I))
      //   FF1 = FF1 + muvec(il2(I) + 1) * xx2(ix3(I))
      //   FF1 = FF1 + lacvec(il1(I)) * nn(in1(I)) * xx1(ix1(I))
      //   FF1 = FF1 + muvec(il2(I)) * nn(in2ix2(I)) * xx1(in2ix2(I))
      //   FFF = (muvec(il3in3(I)) + lacvec(il3in3(I)))
      //   FF1 = FF1 - FFF * nn(il3in3(I)) * xx1(ix3(I))
      //   FF1 = FF1 - gamvec(il3in3(I)) * xx1(ix3(I))
      //   dConc(I) = FF1
      //   FF1 = gamvec(il3in3(I)) * xx1(ix3(I))
      //   FF1 = FF1 + lacvec(il1(I) + 1) * nn(in1(I)) * xx2(ix1(I))
      //   FF1 = FF1 + muvec(il2(I)+1) * nn(in2ix2(I)) * xx2(in2ix2(I))
      //   FFF = muvec(il3in3(I) + 1) + lacvec(il3in3(I) + 1)
      //   FF1 = FF1 - FFF * nn(il3in3(I) + 1) * xx2(ix3(I))
      //   FF1 = FF1 - laavec(il3in3(I) + 1) * xx2(ix3(I))
      //   dConc(N + I) = FF1
      // ENDDO
      // using views into output vector:
      auto dx1 = dx.data();
      auto dx2 = dx1 + p_.lx;
      for (int i = 0; i < p_.lx; ++i) {
        dx1[i] = laavec[il1 + i + 1] * xx2[ix1 + i]
               + lacvec[il4 + i + 1] * xx2[ix4 + i]
               + muvec[il2 + i + 1] * xx2[ix3 + i]
               + lacvec[il1 + i] * nn[in1 + i] * xx1[ix1 + i]
               + muvec[il2 + i] * nn[in2ix2 + i] * xx1[in2ix2 + i]
               - (muvec[il3in3 + i] + lacvec[il3in3 + i]) * nn[il3in3 + i] * xx1[ix3 + i]
               - gamvec[il3in3 + i] * xx1[ix3 + i];
        dx2[i] = gamvec[il3in3 + i] * xx1[ix3 + i]
               + lacvec[il1 + i + 1] * nn[in1 + i] * xx2[ix1 + i]
               + muvec[il2 + i + 1] * nn[in2ix2 + i] * xx2[in2ix2 + i]
               - (muvec[il3in3 + 1 + i] + lacvec[il3in3 + 1 + i]) * nn[il3in3 + i + 1] * xx2[ix3 + i]
               - laavec[il3in3 + i] * xx2[ix3 + i];
      }

      // IF(kk .EQ. 1) THEN
      //   dConc(1) = dConc(1) + laavec(il3in3(1)) * xx3
      //   dConc(2) = dConc(2) + 2 * lacvec(il3in3(1)) * xx3
      // ENDIF
      if (1 == p_.kk) {
        dx1[0] += laavec[il3in3] * xx3;
        dx2[1] += 2.0 * lacvec[il3in3] * xx3;
      }
    }

  private:
    const param_t p_;
  };


  class cpp_daisie_cs_runmod_2
  {
  public:
    cpp_daisie_cs_runmod_2(param_t&& p) :
      p_(p)
    {
    }

    // odeint interface
    void operator()(const state_type& x, state_type& dx, double) const
    {
      if (++p_.steps > max_steps) throw std::runtime_error("cpp_daisie_cs_runmod_2: too many steps");

      // xx1 = c(0,0,x[1:lx],0)
      // xx2 = c(0,0,x[(lx + 1):(2 * lx)],0)
      // xx3 = c(0,0,x[(2 * lx + 1):(3 * lx)],0)
      // using padded views instead of vectors:
      const auto xx1 = padded_vector_view(2, x.data(), p_.lx);
      const auto xx2 = padded_vector_view(2, x.data() + p_.lx, p_.lx);
      const auto xx3 = padded_vector_view(2, x.data() + 2 * p_.lx, p_.lx);

      // DO I = 1, N + 4 + 2 * kk
      //   laavec(I) = P(I)
      //   lacvec(I) = P(I + N + 4 + 2 * kk)
      //   muvec(I)  = P(I + 2 * (N + 4 + 2 * kk))
      //   gamvec(I) = P(I + 3 * (N + 4 + 2 * kk))
      //   nn(I)     = P(I + 4 * (N + 4 + 2 * kk))
      // ENDDO
      // using views instead of vectors:
      const auto chunk = p_.lx + 4 + 2 * p_.kk;
      const auto laavec = p_.P.data();
      const auto lacvec = p_.P.data() + chunk;
      const auto muvec = p_.P.data() + 2 * chunk;
      const auto gamvec = p_.P.data() + 3 * chunk;
      const auto nn = p_.P.data() + 4 * chunk;

      // DO I = 3, N + 2
      //   il1(I - 2) = I + kk - 1
      //   il2(I - 2) = I + kk + 1
      //   il3in3(I - 2) = I + kk
      //   il4(I - 2) = I + kk - 2
      //   in1(I - 2) = I + 2 * kk - 1
      //   in2ix2(I - 2) = I + 1
      //   in4ix1(I - 2) = I - 1
      //   ix3(I - 2) = I
      //   ix4(I - 2) = I - 2
      // ENDDO
      // using offsets into our views instead of vectors:
      const int il1 = 2 + p_.kk - 1;
      const int il2 = 2 + p_.kk + 1;
      const int il3in3 = 2 + p_.kk;
      const int il4 = 2 + p_.kk - 2;
      const int in1 = 2 + 2 * p_.kk - 1;
      const int in2ix2 = 2 + 1;   // spilt in in2, ix2 at no cost?
      const int in4ix1 = 2 - 1;   // split in in4, ix1 at no cost?
      const int ix3 = 2;
      const int ix4 = 2 - 2;

      // DO I = 1, N
      //   FF1 = laavec(il1(I) + 1) * xx2(in4ix1(I))
      //   FF1 = FF1 + lacvec(il4(I) + 1) * xx2(ix4(I))
      //   FF1 = FF1 + muvec(il2(I) + 1) * xx2(ix3(I))
      //   FF1 = FF1 + lacvec(il1(I)) * nn(in1(I)) * xx1(in4ix1(I))
      //   FF1 = FF1 + muvec(il2(I)) * nn(in2ix2(I)) * xx1(in2ix2(I))
      //   FFF = muvec(il3in3(I)) + lacvec(il3in3(I))
      //   FF1 = FF1 - FFF * nn(il3in3(I)) * xx1(ix3(I))
      //   FF1 = FF1 - gamvec(il3in3(I)) * xx1(ix3(I))
      //   FFF = 0
      //   IF(kk .EQ. 1) THEN
      //     FFF = laavec(il3in3(I)) * xx3(ix3(I))
      //     FFF = FFF + 2 * lacvec(il1(I)) * xx3(in4ix1(I))
      //   ENDIF
      //   dConc(I) = FF1 + FFF
      //   FF1 = gamvec(il3in3(I)) * xx1(ix3(I))
      //   FF1 = FF1 + lacvec(il1(I) + 1) * nn(in1(I)) * xx2(in4ix1(I))
      //   FF1 = FF1 + muvec(il2(I)+1) * nn(in2ix2(I)) * xx2(in2ix2(I))
      //   FFF = muvec(il3in3(I) + 1) + lacvec(il3in3(I) + 1)
      //   FF1 = FF1 - FFF * nn(il3in3(I) + 1) * xx2(ix3(I))
      //   FF1 = FF1 - laavec(il3in3(I) + 1) * xx2(ix3(I))
      //   dConc(N + I) = FF1
      //   FF1 = lacvec(il1(I)) * nn(in4ix1(I)) * xx3(in4ix1(I))
      //   FF1 = FF1 + muvec(il2(I)) * nn(in2ix2(I)) * xx3(in2ix2(I))
      //   FFF = lacvec(il3in3(I)) + muvec(il3in3(I))
      //   FF1 = FF1 - FFF * nn(il3in3(I)) * xx3(ix3(I))
      //   FF1 = FF1-(laavec(il3in3(I))+gamvec(il3in3(I)))*xx3(ix3(I))
      //   dConc(2 * N + I) = FF1
      // ENDDO
      // using views into output vector:
      auto dx1 = dx.data();
      auto dx2 = dx1 + p_.lx;
      auto dx3 = dx2 + p_.lx;
      for (int i = 0; i < p_.lx; ++i) {
        dx1[i] = laavec[il1 + i + 1] * xx2[in4ix1 + i]
               + lacvec[il4 + i + 1] * xx2[ix4 + i]
               + muvec[il2 + i + 1] * xx2[ix3 + i]
               + lacvec[il1 + i] * nn[in1 + i] * xx1[in4ix1 + i]
               + muvec[il2 + i] * nn[in2ix2 + i] * xx1[in2ix2 + i]
               - (muvec[il3in3 + i] + lacvec[il3in3 + i]) * nn[il3in3 + i] * xx1[ix3 + i]
               - gamvec[il3in3 + i] * xx1[ix3 + i];
        if (1 == p_.kk) {
          dx1[i] += laavec[il3in3 + i] * xx3[ix3 + i] + 2.0 * lacvec[il1 + i] * xx3[in4ix1 + i];

        }
        dx2[i] = gamvec[il3in3 + i] * xx1[ix3 + i]
               + lacvec[il1 + i + 1] * nn[in1 + i] * xx2[in4ix1 + i]
               + muvec[il2 + i + 1] * nn[in2ix2 + i] * xx2[in2ix2 + i]
               - (muvec[il3in3 + 1 + i] + lacvec[il3in3 + 1 + i]) * nn[il3in3 + i + 1] * xx2[ix3 + i]
               - laavec[il3in3 + i] * xx2[ix3 + i];
        dx3[i] = lacvec[il1 + i] * nn[in4ix1 + i] * xx3[in4ix1 + i]
               + muvec[il2 + i] * nn[in2ix2 + i] * xx3[in2ix2 + i]
               - (lacvec[il3in3 + i] + muvec[il3in3 + i]) * nn[il3in3 + i] * xx3[ix3 + i]
               - (laavec[il3in3 + i] + gamvec[il3in3 + i]) * xx3[ix3 + i];
      }
    }

  private:
    const param_t p_;
  };

} // anonymous namespace


//' Driver for the boost::odeint solver
//'
//' @name daisie_odeint_cs
RcppExport SEXP daisie_odeint_cs(SEXP rrunmod, SEXP ry, SEXP rtimes, SEXP rlx, SEXP rkk, SEXP rpar, SEXP Stepper, SEXP atolint, SEXP reltolint) {
BEGIN_RCPP
  Rcpp::RObject rcpp_result_gen;
  Rcpp::RNGScope rcpp_rngScope_gen;
  auto runmod = as<std::string>(rrunmod);
  auto y = as<std::vector<double>>(ry);
  auto times = as<std::vector<double>>(rtimes);
  auto lx = as<int>(rlx);
  auto kk = as<int>(rkk);
  auto stepper = as<std::string>(Stepper);
  auto atol = as<double>(atolint);
  auto rtol = as<double>(reltolint);

  auto p = param_t(lx, kk, as<state_type>(rpar));
  if (runmod == "daisie_runmod") {
    cpp_daisie_cs_runmod rhs(std::move(p));
    daisie_odeint::integrate(stepper, std::ref(rhs), y, times[0], times[1], atol, rtol);
  }
  else if (runmod == "daisie_runmod2") {
    cpp_daisie_cs_runmod_2 rhs(std::move(p));
    daisie_odeint::integrate(stepper, std::ref(rhs), y, times[0], times[1], atol, rtol);
  }
  else {
    throw std::runtime_error("daisie_odeint_cs: unknown runmod");
  }

  rcpp_result_gen = y;
  return rcpp_result_gen;
END_RCPP
}
