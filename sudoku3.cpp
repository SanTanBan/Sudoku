// Copyright (C) 2005, International Business Machines
// Corporation and others.  All Rights Reserved.
// This code is licensed under the terms of the Eclipse Public License (EPL).

#include <cassert>
#include <iomanip>

#include "CoinPragma.hpp"
// For Branch and bound
#include "OsiSolverInterface.hpp"
#include "CbcModel.hpp"
#include "CbcSolver.hpp"
#include "CoinModel.hpp"
// For all different
#include "CbcBranchCut.hpp"
#include "CbcBranchActual.hpp"
#include "CbcBranchAllDifferent.hpp"
#include "CbcCutGenerator.hpp"
#include "CglAllDifferent.hpp"
#include "OsiClpSolverInterface.hpp"
#include "CglStored.hpp"
#include "CbcBranchLotsize.hpp"

#include "CoinTime.hpp"

/************************************************************************

This shows how we can define a new branching method to solve problems with
all different constraints.

We are going to solve a sudoku problem such as

1, , ,4, , ,7, ,
 ,2, , ,5, , ,8,
8,7,3, , ,6, , ,9
4, , ,7, , ,1, ,
 ,5, , ,8, , ,2,
 , ,6, ,4,9, , ,3
7, , ,1, , ,4, ,
 ,8, ,6,2, , ,5,
 , ,9, ,7,3, ,1,6

The input should be exported from spreadsheet as a csv file where cells are
empty unless value is known.

We set up a fake objective and simple constraints to say sum must be 45

and then we add all different branching (CbcBranchAllDifferent) 
and all different cuts (to fix variables) (CglAllDifferent).

CbcBranchAllDifferent is really just an example of a cut branch.  If we wish to stop x==y
then we can have two branches - one x <= y-1 and the other x >= y+1.  It should be easy for the user to 
make up similar cut branches for other uses.

Note - this is all we need to solve most 9 x 9 puzzles because they seem to solve
at root node or easily.  To solve 16 x 16 puzzles we need more.  All different cuts
need general integer variables to be fixed while we can branch so they are just at bounds.  
To get round that we can introduce extra 0-1 variables such that general integer x = sum j * delta j
and then do N way branching on these (CbcNWay) so that we fix one delta j to 1.  At the same time we use the
new class CbcConsequence (used in N way branching) which when delta j goes to 1 fixes other variables.
So it will fix x to the correct value and while we are at it we can fix some delta variables in other
sets to zero (as per all different rules).  Finally as well as storing the instructions which say if 
delta 11 is 1 then delta 21 is 0 we can also add this in as a cut using new trivial cut class
CglStored.

************************************************************************/
/** Lotsize class modified for branching */

class CbcLotsizeAlt : public CbcLotsize {

public:
  // Default Constructor
  CbcLotsizeAlt();

  /* Useful constructor - passed model index.
       Also passed valid values - if range then pairs
    */
  CbcLotsizeAlt(CbcModel *model, int iColumn,
    int numberPoints, const double *points, bool range = false);

  // Copy constructor
  CbcLotsizeAlt(const CbcLotsizeAlt &);

  /// Clone
  virtual CbcObject *clone() const;

  // Assignment operator
  CbcLotsizeAlt &operator=(const CbcLotsizeAlt &rhs);

  // Destructor
  ~CbcLotsizeAlt();

  /// Infeasibility - large is 0.5
  virtual double infeasibility(const OsiBranchingInformation *info,
    int &preferredWay) const;
  /// Creates a branching object
  virtual CbcBranchingObject *createCbcBranch(OsiSolverInterface *solver, const OsiBranchingInformation *info, int way);


private:
  /// data
};
class CbcLotsizeBranchingObjectAlt : public CbcLotsizeBranchingObject {

public:
  /// Default constructor
  CbcLotsizeBranchingObjectAlt();

  /** Create a lotsize floor/ceiling branch object

      Specifies a simple two-way branch. Let \p value = x*. One arm of the
      branch will be is lb <= x <= valid range below(x*), the other valid range above(x*) <= x <= ub.
      Specify way = -1 to set the object state to perform the down arm first,
      way = 1 for the up arm.
    */
  CbcLotsizeBranchingObjectAlt(CbcModel *model, int variable,
    int way, double value, const CbcLotsize *lotsize);

  /** Create a degenerate branch object

      Specifies a `one-way branch'. Calling branch() for this object will
      always result in lowerValue <= x <= upperValue. Used to fix in valid range
    */

  CbcLotsizeBranchingObjectAlt(CbcModel *model, int variable, int way,
    double lowerValue, double upperValue);

  /// Copy constructor
  CbcLotsizeBranchingObjectAlt(const CbcLotsizeBranchingObjectAlt &);

  /// Assignment operator
  CbcLotsizeBranchingObjectAlt &operator=(const CbcLotsizeBranchingObjectAlt &rhs);

  /// Clone
  virtual CbcBranchingObject *clone() const;

  /// Destructor
  virtual ~CbcLotsizeBranchingObjectAlt();

  using CbcBranchingObject::branch;
  /** \brief Sets the bounds for the variable according to the current arm
           of the branch and advances the object state to the next arm.
    */
  virtual double branch();

  using CbcBranchingObject::print;
  /** \brief Print something about branch - only if log level high
    */
  //virtual void print();

  /** Return the type (an integer identifier) of \c this */
  //virtual CbcBranchObjType type() const
  // {
  //return LotsizeBranchObj;
  //}

  // LL: compareOriginalObject can be inherited from the CbcBranchingObject
  // since variable_ uniquely defines the lot sizing object.

  /** Compare the \c this with \c brObj. \c this and \c brObj must be os the
        same type and must have the same original object, but they may have
        different feasible regions.
        Return the appropriate CbcRangeCompare value (first argument being the
        sub/superset if that's the case). In case of overlap (and if \c
        replaceIfOverlap is true) replace the current branching object with one
        whose feasible region is the overlap.
     */
  //virtual CbcRangeCompare compareBranchingObject(const CbcBranchingObject *brObj, const bool replaceIfOverlap = false);
};

/* Return non-zero to return quickly */
static int callBack(CbcModel *model, int whereFrom)
{
  int returnCode = 0;
  switch (whereFrom) {
  case 1:
  case 2:
    if (!model->status() && model->secondaryStatus())
      returnCode = 1;
    break;
  case 3:
    {
      // Add in lot sizing
      OsiSolverInterface * solver = model->solver();
      int numberColumns = solver->getNumCols();
      const double * lower = solver->getColLower();
      const double * upper = solver->getColUpper();
      // get row copy
      const CoinPackedMatrix *matrix = solver->getMatrixByRow();
      const double *element = matrix->getElements();
      const int *column = matrix->getIndices();
      const CoinBigIndex *rowStart = matrix->getVectorStarts();
      const int *rowLength = matrix->getVectorLengths();
      // by column
      const CoinPackedMatrix *matrix2 = solver->getMatrixByCol();
      const int *row = matrix2->getIndices();
      const CoinBigIndex *columnStart = matrix2->getVectorStarts();
      const int *columnLength = matrix2->getVectorLengths();
      CbcObject **objects = new CbcObject *[numberColumns];
      int size = sqrt(numberColumns);
      double values[100];
      int coeff = 1;
      for (int i=0;i<size;i++) {
	values[i]=coeff;
	coeff *= 2;
      }
      int numberLot = 0;
      for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
	if (upper[iColumn]>lower[iColumn]) {
	  // create object
	  double pairs[200];
	  for (int i=0;i<size;i++) {
	    pairs[2*i] = values[i];
	    pairs[2*i+1] = values[i];
	  }
	  // take out
	  for (CoinBigIndex i=columnStart[iColumn];
	       i<columnStart[iColumn]+columnLength[iColumn];i++) {
	    int iRow = row[i];
	    for (CoinBigIndex j=rowStart[iRow];
		 j<rowStart[iRow]+rowLength[iRow];j++) {
	      int jColumn = column[j];
	      if (upper[jColumn]==lower[jColumn]) {
		// take out that size
		double bound = upper[jColumn];
		for (int i=0;i<size;i++) {
		  if (pairs[2*i] == bound) {
		    pairs[2*i] = -1.0;
		    break;
		  }
		}
	      }
	    }
	  }
	  int n = 0;
	  double smallest = 1.0e10;
	  double largest = -1.0;
	  for (int i=0;i<size;i++) {
	    if (pairs[2*i]>=0.0) {
	      smallest = CoinMin(smallest,pairs[2*i]);
	      largest = CoinMax(largest,pairs[2*i]);
	      pairs[2*n] = pairs[2*i];
	      pairs[2*n+1] = pairs[2*i];
	      n++;
	    }
	  }
	  solver->setColLower(iColumn,smallest);
	  solver->setColUpper(iColumn,largest);
	  objects[numberLot++] = new CbcLotsizeAlt(model, iColumn, n, pairs, true);
	}
      }
      int numberObjects = model->numberObjects();
      model->addObjects(numberLot, objects);
      for (int iColumn = 0; iColumn < numberLot; iColumn++)
	delete objects[iColumn];
      delete[] objects;
      // say stop after solution
      model->setMaximumSolutions(1);
      // Set priorities
      OsiObject ** objectsNow = model->objects();
      for (int i=0;i<numberObjects;i++)
	objectsNow[i]->setPriority(2000);
      for (int i=numberObjects;i<numberObjects+numberLot;i++)
	objectsNow[i]->setPriority(1000);
    }
    break;
  case 4:
    // If not good enough could skip postprocessing
    break;
  case 5:
    break;
  default:
    abort();
  }
  return returnCode;
}
// Default Constructor
CbcLotsizeAlt::CbcLotsizeAlt()
  : CbcLotsize()
{
}
/* Useful constructor - passed model index.
   Also passed valid values - if range then pairs
*/
CbcLotsizeAlt::CbcLotsizeAlt(CbcModel *model, int iColumn,
			     int numberPoints, const double *points,
			     bool range)
  : CbcLotsize(model, iColumn, numberPoints, points, range)
{
}
// Copy constructor
CbcLotsizeAlt::CbcLotsizeAlt(const CbcLotsizeAlt & rhs)
: CbcLotsize(rhs)
{
}
// Clone
CbcObject *
CbcLotsizeAlt::clone() const
{
  return new CbcLotsizeAlt(*this);
}
// Assignment operator
//CbcLotsizeAlt::CbcLotsizeAlt &operator=(const CbcLotsizeAlt &rhs);
//: CbcLotsize(rhs)/
//{
//}
// Destructor
CbcLotsizeAlt::~CbcLotsizeAlt()
{
  //  delete[] bound_;
  //bound_=NULL;
}
// Infeasibility - large is 0.5
double CbcLotsizeAlt::infeasibility(const OsiBranchingInformation *info,
    int &preferredWay) const
{
  OsiSolverInterface *solver = model_->solver();
  const double *solution = model_->testSolution();
  const double *lower = solver->getColLower();
  const double *upper = solver->getColUpper();
  double value = solution[columnNumber_];
  value = CoinMax(value, lower[columnNumber_]);
  value = CoinMin(value, upper[columnNumber_]);
  double integerTolerance = model_->getDblParam(CbcModel::CbcIntegerTolerance);
  /*printf("%d %g %g %g %g\n",columnNumber_,value,lower[columnNumber_],
      solution[columnNumber_],upper[columnNumber_]);*/
  assert(value >= bound_[0] - integerTolerance
    && value <= bound_[rangeType_ * numberRanges_ - 1] + integerTolerance);
  double infeasibility = 0.0;
  bool feasible = findRange(value);
  if (!feasible) {
    if (rangeType_ == 1) {
      if (value - bound_[range_] < bound_[range_ + 1] - value) {
        preferredWay = -1;
        infeasibility = value - bound_[range_];
      } else {
        preferredWay = 1;
        infeasibility = bound_[range_ + 1] - value;
      }
    } else {
      // ranges
      if (value - bound_[2 * range_ + 1] < bound_[2 * range_ + 2] - value) {
        preferredWay = -1;
        infeasibility = value - bound_[2 * range_ + 1];
      } else {
        preferredWay = 1;
        infeasibility = bound_[2 * range_ + 2] - value;
      }
    }
  } else {
    // always satisfied
    preferredWay = -1;
  }
  if (infeasibility < integerTolerance)
    infeasibility = 0.0;
  else
    infeasibility /= largestGap_;
#ifdef CBC_PRINT
  printLotsize(value, infeasibility, 1);
#endif
  // branch on large values first
  infeasibility *= value;
  return infeasibility;
}
CbcBranchingObject *
CbcLotsizeAlt::createCbcBranch(OsiSolverInterface *solver, const OsiBranchingInformation * /*info*/, int way)
{
  //OsiSolverInterface * solver = model_->solver();
  const double *solution = model_->testSolution();
  const double *lower = solver->getColLower();
  const double *upper = solver->getColUpper();
  double value = solution[columnNumber_];
  value = CoinMax(value, lower[columnNumber_]);
  value = CoinMin(value, upper[columnNumber_]);
  assert(!findRange(value));
  return new CbcLotsizeBranchingObjectAlt(model_, columnNumber_, way,
    value, this);
}
// Default Constructor
CbcLotsizeBranchingObjectAlt::CbcLotsizeBranchingObjectAlt()
  : CbcLotsizeBranchingObject()
{
}

// Useful constructor
CbcLotsizeBranchingObjectAlt::CbcLotsizeBranchingObjectAlt(CbcModel *model,
  int variable, int way, double value,
  const CbcLotsize *lotsize)
  : CbcLotsizeBranchingObject(model, variable, way, value,lotsize)
{
}
// Useful constructor for fixing
CbcLotsizeBranchingObjectAlt::CbcLotsizeBranchingObjectAlt(CbcModel *model,
  int variable, int way,
  double lowerValue,
  double upperValue)
  : CbcLotsizeBranchingObject(model, variable, way, lowerValue,upperValue)
{
}

// Copy constructor
CbcLotsizeBranchingObjectAlt::CbcLotsizeBranchingObjectAlt(const CbcLotsizeBranchingObjectAlt &rhs)
  : CbcLotsizeBranchingObject(rhs)
{
}

// Assignment operator
CbcLotsizeBranchingObjectAlt &
CbcLotsizeBranchingObjectAlt::operator=(const CbcLotsizeBranchingObjectAlt &rhs)
{
  if (this != &rhs) {
    CbcBranchingObject::operator=(rhs);
    down_[0] = rhs.down_[0];
    down_[1] = rhs.down_[1];
    up_[0] = rhs.up_[0];
    up_[1] = rhs.up_[1];
  }
  return *this;
}
CbcBranchingObject *
CbcLotsizeBranchingObjectAlt::clone() const
{
  return (new CbcLotsizeBranchingObjectAlt(*this));
}

// Destructor
CbcLotsizeBranchingObjectAlt::~CbcLotsizeBranchingObjectAlt()
{
}

/*
  Perform a branch by adjusting the bounds of the specified variable. Note
  that each arm of the branch advances the object to the next arm by
  advancing the value of way_.

  Providing new values for the variable's lower and upper bounds for each
  branching direction gives a little bit of additional flexibility and will
  be easily extensible to multi-way branching.
*/
double
CbcLotsizeBranchingObjectAlt::branch()
{
  decrementNumberBranchesLeft();
  int iColumn = variable_;
  if (way_ < 0) {
#ifndef NDEBUG
    {
      double olb, oub;
      olb = model_->solver()->getColLower()[iColumn];
      oub = model_->solver()->getColUpper()[iColumn];
#ifdef CBC_DEBUG
      printf("branching down on var %d: [%g,%g] => [%g,%g]\n",
        iColumn, olb, oub, down_[0], down_[1]);
#endif
      assert(olb < down_[0] + 1.0e-7 && oub > down_[1] - 1.0e-7);
    }
#endif
    model_->solver()->setColLower(iColumn, down_[0]);
    model_->solver()->setColUpper(iColumn, down_[1]);
    way_ = 1;
  } else {
#ifndef NDEBUG
    {
      double olb, oub;
      olb = model_->solver()->getColLower()[iColumn];
      oub = model_->solver()->getColUpper()[iColumn];
#ifdef CBC_DEBUG
      printf("branching up on var %d: [%g,%g] => [%g,%g]\n",
        iColumn, olb, oub, up_[0], up_[1]);
#endif
      assert(olb < up_[0] + 1.0e-7 && oub > up_[1] - 1.0e-7);
    }
#endif
    model_->solver()->setColLower(iColumn, up_[0]);
    model_->solver()->setColUpper(iColumn, up_[1]);
    way_ = -1; // Swap direction
  }
  return 0.0;
}
int main(int argc, const char *argv[])
{
  // Get data
  std::string fileName = "./sudoku_sample.csv";
  if (argc >= 2)
    fileName = argv[1];
  FILE *fp = fopen(fileName.c_str(), "r");
  if (!fp) {
    printf("Unable to open file %s\n", fileName.c_str());
    exit(0);
  }
#define MAX_SIZE 16
  int valueOffset = 1;
  double lo[MAX_SIZE * MAX_SIZE], up[MAX_SIZE * MAX_SIZE];
  int alldiff[MAX_SIZE * MAX_SIZE];
  char line[80];
  int row, column;
  /*************************************** 
     Read .csv file and see if 9 or 16 Su Doku
  ***************************************/
  int size = 9;
  for (row = 0; row < size; row++) {
    fgets(line, 80, fp);
    // Get size of sudoku puzzle (9 or 16) temp allow 4
    if (!row) {
      int get = 0;
      size = 1;
      while (line[get] >= 32) {
        if (line[get] == ',')
          size++;
        get++;
      }
      assert(size == 9 || size == 16 || size == 4);
      printf("Solving Su Doku of size %d\n", size);
      if (size == 16)
        valueOffset = 0;
    }
    int get = 0;
    for (column = 0; column < size; column++) {
      lo[size * row + column] = valueOffset;
      up[size * row + column] = valueOffset - 1 + size;
      alldiff[size * row + column] = -1;
      if (line[get] != ',' && line[get] >= 32) {
        // skip blanks
        if (line[get] == ' ') {
          get++;
          continue;
        }
        int value = line[get] - '0';
        if (size == 9) {
          assert(value >= 1 && value <= 9);
        } else if (size == 4) {
          assert(value >= 1 && value <= 4);
        } else {
          assert(size == 16);
          if (value < 0 || value > 9) {
            if (line[get] == '"') {
              get++;
              value = 10 + line[get] - 'A';
              if (value < 10 || value > 15) {
                value = 10 + line[get] - 'a';
              }
              get++;
            } else {
              value = 10 + line[get] - 'A';
              if (value < 10 || value > 15) {
                value = 10 + line[get] - 'a';
              }
            }
          }
          assert(value >= 0 && value <= 15);
        }
        lo[size * row + column] = value;
        up[size * row + column] = value;
	alldiff[size * row + column] = value;
        get++;
      }
      get++;
    }
  }
  fclose(fp);
  int block_size = (int)sqrt((double)size);
  /*************************************** 
     Now build rules for all different
     3*9 or 3*16 sets of variables
     Number variables by row*size+column
  ***************************************/
  int starts[3 * MAX_SIZE + 1];
  int which[3 * MAX_SIZE * MAX_SIZE];
  int put = 0;
  int set = 0;
  starts[0] = 0;
  // By row
  for (row = 0; row < size; row++) {
    for (column = 0; column < size; column++)
      which[put++] = row * size + column;
    starts[set + 1] = put;
    set++;
  }
  // By column
  for (column = 0; column < size; column++) {
    for (row = 0; row < size; row++)
      which[put++] = row * size + column;
    starts[set + 1] = put;
    set++;
  }
  // By box
  for (row = 0; row < size; row += block_size) {
    for (column = 0; column < size; column += block_size) {
      for (int row2 = row; row2 < row + block_size; row2++) {
        for (int column2 = column; column2 < column + block_size; column2++)
          which[put++] = row2 * size + column2;
      }
      starts[set + 1] = put;
      set++;
    }
  }
  OsiClpSolverInterface solver1;

  /*************************************** 
     Create model
     Set variables to be general integer variables although
     priorities probably mean that it won't matter
  ***************************************/
  CoinModel build;
  // Columns
  char name[4];
  for (row = 0; row < size; row++) {
    for (column = 0; column < size; column++) {
      if (row < 10) {
        if (column < 10)
          sprintf(name, "X%d%d", row, column);
        else
          sprintf(name, "X%d%c", row, 'A' + (column - 10));
      } else {
        if (column < 10)
          sprintf(name, "X%c%d", 'A' + (row - 10), column);
        else
          sprintf(name, "X%c%c", 'A' + (row - 10), 'A' + (column - 10));
      }
      double value = CoinDrand48() * 100.0;
      build.addColumn(0, NULL, NULL, lo[size * row + column],
        up[size * row + column], value, name, true);
    }
  }
  /*************************************** 
     Now add in extra variables for N way branching
  ***************************************/
  for (row = 0; row < size; row++) {
    for (column = 0; column < size; column++) {
      int iColumn = size * row + column;
      double value = lo[iColumn];
      if (value < up[iColumn]) {
        for (int i = 0; i < size; i++)
          build.addColumn(0, NULL, NULL, 0.0, 1.0, 0.0);
      } else {
        // fixed
        // obviously could do better  if we missed out variables
        int which = ((int)value) - valueOffset;
        for (int i = 0; i < size; i++) {
          if (i != which)
            build.addColumn(0, NULL, NULL, 0.0, 0.0, 0.0);
          else
            build.addColumn(0, NULL, NULL, 0.0, 1.0, 0.0);
        }
      }
    }
  }

  /*************************************** 
     Now rows
  ***************************************/
  double values[] = { 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0 };
  int indices[MAX_SIZE + 1];
  double rhs;
  if (size == 9)
    rhs = 45.0;
  else if (size == 16)
    rhs = 136.0; // was 120 ???
  else if (size == 4)
    rhs = 10.0;
  for (row = 0; row < 3 * size; row++) {
    int iStart = starts[row];
    for (column = 0; column < size; column++)
      indices[column] = which[column + iStart];
    build.addRow(size, indices, values, rhs, rhs);
  }
  double values2[MAX_SIZE + 1];
  values2[0] = -1.0;
  for (row = 0; row < size; row++)
    values2[row + 1] = row + valueOffset;
  // Now add rows for extra variables
  for (row = 0; row < size; row++) {
    for (column = 0; column < size; column++) {
      int iColumn = row * size + column;
      int base = size * size + iColumn * size;
      indices[0] = iColumn;
      for (int i = 0; i < size; i++)
        indices[i + 1] = base + i;
      build.addRow(size + 1, indices, values2, 0.0, 0.0);
    }
  }
  solver1.loadFromCoinModel(build);
  //build.writeMps("xx.mps");
  int saveLo[256];
  int saveUp[256];
  for (int i=0;i<size*size;i++) {
    saveLo[i] = lo[i];
    saveUp[i] = up[i];
  }
  printf("Starting all different version\n");
  double time1 = CoinCpuTime();
  solver1.initialSolve();
  CbcModel model(solver1);
  model.solver()->setHintParam(OsiDoReducePrint, true, OsiHintTry);
  model.solver()->setHintParam(OsiDoScale, false, OsiHintTry);
  /*************************************** 
    Add in All different cut generator and All different branching
    So we will have integers then cut branching then N way branching
    in reverse priority order
  ***************************************/

  // Cut generator
  CglAllDifferent allDifferent(3 * size, starts, which);
  model.addCutGenerator(&allDifferent, -99, "allDifferent");
  model.cutGenerator(0)->setWhatDepth(5);
  CbcObject **objects = new CbcObject *[4 * size * size];
  int nObj = 0;
  for (row = 0; row < 3 * size; row++) {
    int iStart = starts[row];
    objects[row] = new CbcBranchAllDifferent(&model, size, which + iStart);
    objects[row]->setPriority(2000 + nObj); // do after rest satisfied
    nObj++;
  }
  /*************************************** 
     Add in N way branching and while we are at it add in cuts
  ***************************************/
  CglStored stored;
  for (row = 0; row < size; row++) {
    for (column = 0; column < size; column++) {
      int iColumn = row * size + column;
      int base = size * size + iColumn * size;
      int i;
      for (i = 0; i < size; i++)
        indices[i] = base + i;
      CbcNWay *obj = new CbcNWay(&model, size, indices, nObj);
      int seq[200];
      int newUpper[200];
      memset(newUpper, 0, sizeof(newUpper));
      for (i = 0; i < size; i++) {
        int state = 9999;
        int one = 1;
        int nFix = 1;
        // Fix real variable
        seq[0] = iColumn;
        newUpper[0] = valueOffset + i;
        int kColumn = base + i;
        int j;
        // same row
        for (j = 0; j < size; j++) {
          int jColumn = row * size + j;
          int jjColumn = size * size + jColumn * size + i;
          if (jjColumn != kColumn) {
            seq[nFix++] = jjColumn; // must be zero
          }
        }
        // same column
        for (j = 0; j < size; j++) {
          int jColumn = j * size + column;
          int jjColumn = size * size + jColumn * size + i;
          if (jjColumn != kColumn) {
            seq[nFix++] = jjColumn; // must be zero
          }
        }
        // same block
        int kRow = row / block_size;
        kRow *= block_size;
        int kCol = column / block_size;
        kCol *= block_size;
        for (j = kRow; j < kRow + block_size; j++) {
          for (int jc = kCol; jc < kCol + block_size; jc++) {
            int jColumn = j * size + jc;
            int jjColumn = size * size + jColumn * size + i;
            if (jjColumn != kColumn) {
              seq[nFix++] = jjColumn; // must be zero
            }
          }
        }
        // seem to need following?
        const int *upperAddress = newUpper;
        const int *seqAddress = seq;
        CbcFixVariable fix(1, &state, &one, &upperAddress, &seqAddress, &nFix, &upperAddress, &seqAddress);
        obj->setConsequence(indices[i], fix);
        // Now do as cuts
        for (int kk = 1; kk < nFix; kk++) {
          int jColumn = seq[kk];
          int cutInd[2];
          cutInd[0] = kColumn;
          if (jColumn > kColumn) {
            cutInd[1] = jColumn;
            stored.addCut(-COIN_DBL_MAX, 1.0, 2, cutInd, values);
          }
        }
      }
      objects[nObj] = obj;
      objects[nObj]->setPriority(nObj);
      nObj++;
    }
  }
  model.addObjects(nObj, objects);
  for (row = 0; row < nObj; row++)
    delete objects[row];
  delete[] objects;
  model.messageHandler()->setLogLevel(1);
  model.addCutGenerator(&stored, 1, "stored");
  // Say we want timings
  int numberGenerators = model.numberCutGenerators();
  int iGenerator;
  for (iGenerator = 0; iGenerator < numberGenerators; iGenerator++) {
    CbcCutGenerator *generator = model.cutGenerator(iGenerator);
    generator->setTiming(true);
  }
  // Set this to get all solutions (all ones in newspapers should only have one)
  //model.setCutoffIncrement(-1.0e6);
  // set this to exit without looking any further
  model.setMaximumSolutions(1);
  /*************************************** 
     Do branch and bound
  ***************************************/
  // Do complete search
  model.branchAndBound();
  std::cout << "took " << CoinCpuTime() - time1 << " seconds, "
            << model.getNodeCount() << " nodes with objective "
            << model.getObjValue()
            << (!model.status() ? " Finished" : " Not finished")
            << std::endl;
  // statistics
  double timeA = CoinCpuTime()-time1;
  int numberRowsA = model.getNumRows();
  int numberColumnsA = model.getNumCols();
  time1 = CoinCpuTime();
  /*************************************** 
     Print solution and check it is feasible
     We could modify output so could be imported by spreadsheet
  ***************************************/
  if (model.getMinimizationObjValue() < 1.0e50) {

    const double *solution = model.bestSolution();
    int put = 0;
    for (row = 0; row < size; row++) {
      for (column = 0; column < size; column++) {
        int value = (int)floor(solution[row * size + column] + 0.5);
        assert(value >= lo[put] && value <= up[put]);
        // save for later test
        lo[put++] = value;
        printf("%d ", value);
      }
      printf("\n");
    }
    // check valid
    bool valid = true;
    // By row
    for (row = 0; row < size; row++) {
      put = 0;
      for (column = 0; column < size; column++)
        which[put++] = row * size + column;
      assert(put == size);
      int i;
      for (i = 0; i < put; i++)
        which[i] = (int)lo[which[i]];
      std::sort(which, which + put);
      int last = valueOffset - 1;
      for (i = 0; i < put; i++) {
        int value = which[i];
        if (value != last + 1)
          valid = false;
        last = value;
      }
    }
    // By column
    for (column = 0; column < size; column++) {
      put = 0;
      for (row = 0; row < size; row++)
        which[put++] = row * size + column;
      assert(put == size);
      int i;
      for (i = 0; i < put; i++)
        which[i] = (int)lo[which[i]];
      std::sort(which, which + put);
      int last = valueOffset - 1;
      for (i = 0; i < put; i++) {
        int value = which[i];
        if (value != last + 1)
          valid = false;
        last = value;
      }
    }
    // By box
    for (row = 0; row < size; row += block_size) {
      for (column = 0; column < size; column += block_size) {
        put = 0;
        for (int row2 = row; row2 < row + block_size; row2++) {
          for (int column2 = column; column2 < column + block_size; column2++)
            which[put++] = row2 * size + column2;
        }
        assert(put == size);
        int i;
        for (i = 0; i < put; i++)
          which[i] = (int)lo[which[i]];
        std::sort(which, which + put);
        int last = valueOffset - 1;
        for (i = 0; i < put; i++) {
          int value = which[i];
          if (value != last + 1)
            valid = false;
          last = value;
        }
      }
    }
    if (valid) {
      printf("solution is valid\n");
    } else {
      printf("solution is not valid\n");
      abort();
    }
  }
  // Now do using lotsizing
  {
    // put back
    for (int i=0;i<size*size;i++) {
      lo[i] = saveLo[i];
      up[i] = saveUp[i];
    }
    printf("Starting lotsizing version\n");
    int put = 0;
    int set = 0;
    starts[0] = 0;
    // By row
    for (row = 0; row < size; row++) {
      for (column = 0; column < size; column++)
	which[put++] = row * size + column;
      starts[set + 1] = put;
      set++;
    }
    // By column
    for (column = 0; column < size; column++) {
      for (row = 0; row < size; row++)
	which[put++] = row * size + column;
      starts[set + 1] = put;
      set++;
    }
    // By box
    for (row = 0; row < size; row += block_size) {
      for (column = 0; column < size; column += block_size) {
	for (int row2 = row; row2 < row + block_size; row2++) {
	  for (int column2 = column; column2 < column + block_size; column2++)
	    which[put++] = row2 * size + column2;
	}
	starts[set + 1] = put;
	set++;
      }
    }
    OsiClpSolverInterface solver1;
    
    /*************************************** 
     Create model
     Set variables to be general integer variables although
     priorities probably mean that it won't matter
    ***************************************/
    double values[16];
    double lotsizes[16];
    int coeff = 1;
    double rhs = 0.0;
    for (int i=0;i<size;i++) {
      values[i] = 1.0;
      lotsizes[i] = coeff;
      rhs += coeff;
      coeff *= 2;
    }
    CoinModel build;
    // Columns
    char name[4];
    for (row = 0; row < size; row++) {
      for (column = 0; column < size; column++) {
	if (row < 10) {
	  if (column < 10)
	    sprintf(name, "X%d%d", row, column);
	  else
	    sprintf(name, "X%d%c", row, 'A' + (column - 10));
	} else {
	  if (column < 10)
	    sprintf(name, "X%c%d", 'A' + (row - 10), column);
	  else
	    sprintf(name, "X%c%c", 'A' + (row - 10), 'A' + (column - 10));
	}
	double value = CoinDrand48() * 100.0;
	int diffBound = alldiff[size * row + column];
	double lo = 1.0;
	double up = rhs;
	if (diffBound>=0) {
	  lo = lotsizes[diffBound-1]; 
	  up = lotsizes[diffBound-1]; 
	}
	build.addColumn(0, NULL, NULL, lo, up, value, name, true);
      }
    }
    
    /*************************************** 
     Now rows
    ***************************************/
    int indices[MAX_SIZE + 1];
    for (row = 0; row < 3 * size; row++) {
      int iStart = starts[row];
      for (column = 0; column < size; column++)
	indices[column] = which[column + iStart];
      build.addRow(size, indices, values, rhs, rhs);
    }
    solver1.loadFromCoinModel(build);
    //build.writeMps("/tmp/yy.mps");
    // Tell solver to return fast if presolve or initial solve infeasible
    solver1.getModelPtr()->setMoreSpecialOptions(3);
    
    // Messy code below copied from CbcSolver.cpp
    // Pass to Cbc initialize defaults
    CbcModel modelA(solver1);
    CbcSolverUsefulData solverData;
    CbcModel *model = &modelA;
    CbcMain0(modelA,solverData);
    /* Now go into code for standalone solver
       Could copy arguments and add -quit at end to be safe
       but this will do
    */
    const char *argv2[] = { "lotsizing", "-preprocess","off","-solve", "-quit" };
    CbcMain1(3, argv2, modelA, callBack,solverData);
    // Solver was cloned so get current copy
    OsiSolverInterface *solver = model->solver();
    // Print solution if finished
    if (model->getMinimizationObjValue() < 1.0e50) {
      const double *solution = model->bestSolution();
      int put = 0;
      double coeffM[100];
      int coeff = 1;
      for (int i=0;i<size;i++) {
	coeffM[i] = coeff;
	coeff *= 2;
      }
      for (row = 0; row < size; row++) {
	for (column = 0; column < size; column++) {
	  int value = (int)floor(solution[row * size + column] + 0.5);
	  // convert
	  int iValue;
	  for (iValue=1;iValue<=size;iValue++) {
	    if (fabs(value-coeffM[iValue-1])<1.0e-5)
	      break;
	  }
	  assert(iValue >= lo[put] && iValue <= up[put]);
	  // save for later test
	  lo[put++] = iValue;
	  printf("%d ", iValue);
	}
	printf("\n");
      }
      // check valid
      bool valid = true;
      // By row
      for (row = 0; row < size; row++) {
	put = 0;
	for (column = 0; column < size; column++)
	  which[put++] = row * size + column;
	assert(put == size);
	int i;
	for (i = 0; i < put; i++)
	  which[i] = (int)lo[which[i]];
	std::sort(which, which + put);
	int last = valueOffset - 1;
	for (i = 0; i < put; i++) {
	  int value = which[i];
	  if (value != last + 1)
	    valid = false;
	  last = value;
	}
      }
      // By column
      for (column = 0; column < size; column++) {
	put = 0;
	for (row = 0; row < size; row++)
	  which[put++] = row * size + column;
	assert(put == size);
	int i;
	for (i = 0; i < put; i++)
	  which[i] = (int)lo[which[i]];
	std::sort(which, which + put);
	int last = valueOffset - 1;
	for (i = 0; i < put; i++) {
	  int value = which[i];
	  if (value != last + 1)
	    valid = false;
	  last = value;
	}
      }
      // By box
      for (row = 0; row < size; row += block_size) {
	for (column = 0; column < size; column += block_size) {
	  put = 0;
	  for (int row2 = row; row2 < row + block_size; row2++) {
	    for (int column2 = column; column2 < column + block_size; column2++)
	      which[put++] = row2 * size + column2;
	  }
	  assert(put == size);
	  int i;
	  for (i = 0; i < put; i++)
	    which[i] = (int)lo[which[i]];
	  std::sort(which, which + put);
	  int last = valueOffset - 1;
	  for (i = 0; i < put; i++) {
	    int value = which[i];
	    if (value != last + 1)
	      valid = false;
	    last = value;
	  }
	}
      }
      if (valid) {
	printf("solution is valid\n");
      } else {
	printf("solution is not valid\n");
	abort();
      }
    }
    double timeB = CoinCpuTime()-time1;
    int numberRowsB = model->getNumRows();
    int numberColumnsB = model->getNumCols();
    printf("All different %d rows, %d columns took %f8.2 seconds\n",
	   numberRowsA,numberColumnsA,timeA);
    printf("Lot sizing %d rows, %d columns took %f8.2 seconds\n",
	   numberRowsB,numberColumnsB,timeB);
  }
  return 0;
}
