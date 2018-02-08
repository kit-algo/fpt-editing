/** template used by choices.py to force template instanziation */

#include "../../src/choices.hpp"
#include "../../src/Run_impl.hpp"

using M = Options::Modes::{5};
using R = Options::Restrictions::{6};
using C = Options::Conversions::{7};

using GT = Graph::{4}<true>;
using GET = Graph::Matrix<true>;
using FT = Finder::{1}<GT, GET>;
using ST = Selector::{2}<GT, GET, M, R, C>;
using BT = Lower_Bound::{3}<GT, GET, M, R, C>;
using ET = Editor::{0}<FT, GT, GET, M, R, C, ST, BT>;

template class Run<ET, FT, GT, GET, M, R, C, ST, BT>;

using GF = Graph::{4}<false>;
using GEF = Graph::Matrix<false>;
using FF = Finder::{1}<GF, GEF>;
using SF = Selector::{2}<GF, GEF, M, R, C>;
using BF = Lower_Bound::{3}<GF, GEF, M, R, C>;
using EF = Editor::{0}<FF, GF, GEF, M, R, C, SF, BF>;

template class Run<EF, FF, GF, GEF, M, R, C, SF, BF>;
