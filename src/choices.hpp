#ifndef CHOICES_HPP
#define CHOICES_HPP

#include "Options.hpp"

/* List of all includes providing implementations for selectable components */

#include "Editor/ST.hpp"
#include "Editor/MT.hpp"
#include "Finder/Center.hpp"
/*
#include "Finder/Center_Edits.hpp"
#include "Finder/Center_Edits_Sparse.hpp"
#include "Finder/Center_Edits_Sparse_Old.hpp"
//#include "Finder/Linear.hpp"
*/
#include "Consumer/S_First.hpp"
/*
#include "Consumer/S_Least_Unedited.hpp"
#include "Consumer/S_Single.hpp"
#include "Consumer/S_Single_Edits_Sparse.hpp"
#include "Consumer/S_Single_First.hpp"
#include "Consumer/S_Single_First_Edits_Sparse.hpp"
#include "Consumer/S_Single_Heur.hpp"
*/
#include "Consumer/S_Most.hpp"
#include "Consumer/LB_No.hpp"
#include "Consumer/LB_Basic.hpp"
/*
#include "Consumer/LB_Updated.hpp"
#include "Consumer/LB_Min_Deg.hpp"
*/
#include "Consumer/LB_ARW.hpp"
/*
#include "Consumer/LB_Global_ARW.hpp"
#include "Consumer/LB_KaMIS.hpp"
#include "Consumer/LB_PLS.hpp"
#include "Consumer/Counter.hpp"
*/
#include "Graph/Matrix.hpp"

#endif
