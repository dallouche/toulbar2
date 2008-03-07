/*
 * **************** Read wcsp format files **************************
 * 
 */

#include "tb2wcsp.hpp"
#include "tb2enumvar.hpp"
#include "tb2pedigree.hpp"
#include "tb2bep.hpp"
#include "tb2naryconstr.hpp"
#include "tb2randomgen.hpp"


typedef struct {
    EnumeratedVariable *var;
    vector<Cost> costs;
} TemporaryUnaryConstraint;


void WCSP::read_wcsp(const char *fileName)
{
    if (ToulBar2::pedigree) {
      if (!ToulBar2::bayesian) ToulBar2::pedigree->read(fileName, this);
      else ToulBar2::pedigree->read_bayesian(fileName, this);
      return;
    }
	if (ToulBar2::bep) {
	  ToulBar2::bep->read(fileName, this);
	  return;
	}
    string pbname;
    int nbvar,nbval,nbconstr;
    Cost top;
    int i,j,k,t, ic;
    string varname;
    int domsize;
    unsigned int a;
    unsigned int b;
    unsigned int c;
    Cost defval;
    Cost cost;
    int ntuples;
    int arity;
    string funcname;
    Value funcparam1;
	Value funcparam2;
    vector<TemporaryUnaryConstraint> unaryconstrs;
    Cost inclowerbound = MIN_COST;

   
    
    // open the file
    ifstream file(fileName);
    if (!file) {
        cerr << "Could not open file " << fileName << endl;
        exit(EXIT_FAILURE);
    }
    
    // read problem name and sizes
    file >> pbname;
    file >> nbvar;
    file >> nbval;
    file >> nbconstr;
    file >> top;
    if (ToulBar2::verbose) cout << "Read problem: " << pbname << endl;
    
    assert(vars.empty());
    assert(constrs.empty());
    
	Cost K = ToulBar2::costMultiplier;    
	if(top < MAX_COST / K)	top = top * K;
	else top = MAX_COST;
	updateUb(top);

    // read variable domain sizes
    for (i = 0; i < nbvar; i++) {
        string varname;
        varname = to_string(i);
        file >> domsize;
        if (ToulBar2::verbose >= 3) cout << "read variable " << i << " of size " << domsize << endl;
        int theindex = -1;
         
        if (domsize >= 0) theindex = makeEnumeratedVariable(varname,0,domsize-1);
        else theindex = makeIntervalVariable(varname,0,-domsize-1);
        assert(theindex == i);   
    }
    
    // read each constraint
    for (ic = 0; ic < nbconstr; ic++) {
        file >> arity;
        if(arity > MAX_ARITY)  { cerr << "Nary constraints of arity > " << MAX_ARITY << " not supported" << endl; exit(EXIT_FAILURE); }       
        if (!file) {
            cerr << "Warning: EOF reached before reading all the constraints (initial number of constraints too large?)" << endl;
            break;
        }
        if (arity > 3) {
        	int scopeIndex[MAX_ARITY];
			for(i=0;i<arity;i++) {
	            file >> j;
	            scopeIndex[i] = j;
			}     	
            file >> defval;
		    file >> ntuples;
    
		    if((defval != MIN_COST) || (ntuples > 0))           
		    { 
			    Cost tmpcost = defval*K;
			    if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
	            int naryIndex = postNaryConstraint(scopeIndex,arity,tmpcost);
                NaryConstraint *nary = (NaryConstraint *) constrs[naryIndex];

	            char buf[MAX_ARITY];
	            for (t = 0; t < ntuples; t++) {
					for(i=0;i<arity;i++) {
			            file >> j;			            
			            buf[i] = j + CHAR_FIRST;
					}
					buf[i] = '\0';
				    file >> cost;
				    cost = cost * K;
					if (CUT(cost, getUb())) cost *= MEDIUM_COST;
					string tup = buf;
					nary->setTuple(tup, cost, NULL);
	            }
	            //nary->changeDefCost( top );
	            //nary->preprojectall2();
		    }
        } else if (arity == 3) {
            file >> i;
            file >> j;
            file >> k;
        	if ((i == j) || (i == k) || (k == j)) {
    	       cerr << "Error: ternary constraint!" << endl;
               exit(EXIT_FAILURE);
            }
            file >> defval;
            if (defval >= MIN_COST) {
                assert(vars[i]->enumerated());
                assert(vars[j]->enumerated());
                assert(vars[k]->enumerated());
                EnumeratedVariable *x = (EnumeratedVariable *) vars[i];
                EnumeratedVariable *y = (EnumeratedVariable *) vars[j];
                EnumeratedVariable *z = (EnumeratedVariable *) vars[k];
                if (ToulBar2::verbose >= 3) cout << "read ternary constraint " << ic << " on " << i << "," << j << "," << k << endl;
                file >> ntuples;
                vector<Cost> costs;
                for (a = 0; a < x->getDomainInitSize(); a++) {
                    for (b = 0; b < y->getDomainInitSize(); b++) {
	                    for (c = 0; c < z->getDomainInitSize(); c++) {
						    Cost tmpcost = defval*K;
							if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
	                        costs.push_back(tmpcost);
						}
                    }
                }
            	for (t = 0; t < ntuples; t++) {
                    file >> a;
                    file >> b;
                    file >> c;
                    file >> cost;
					Cost tmpcost = cost*K;
					if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
                    costs[a * y->getDomainInitSize() * z->getDomainInitSize() + b * z->getDomainInitSize() + c] = tmpcost;                    
                }
                if(ToulBar2::vac) {
	                for (a = 0; a < x->getDomainInitSize(); a++) {
	                    for (b = 0; b < y->getDomainInitSize(); b++) {
		                    for (c = 0; c < z->getDomainInitSize(); c++) {
		             			Cost co = costs[a * y->getDomainInitSize() * z->getDomainInitSize() + b * z->getDomainInitSize() + c];
		                        histogram(co);
		                    }
	                    }
	                }               	
                }                
                if((defval != MIN_COST) || (ntuples > 0)) postTernaryConstraint(i,j,k,costs);
            }
		} else if (arity == 2) {
            file >> i;
            file >> j;
			if (ToulBar2::verbose >= 3) cout << "read binary constraint " << ic << " on " << i << "," << j << endl;
        	if (i == j) {
    	       cerr << "Error: binary constraint with only one variable in its scope!" << endl;
               exit(EXIT_FAILURE);
            }
            file >> defval;
            if (defval >= MIN_COST) {
                assert(vars[i]->enumerated());
                assert(vars[j]->enumerated());
                EnumeratedVariable *x = (EnumeratedVariable *) vars[i];
                EnumeratedVariable *y = (EnumeratedVariable *) vars[j];
                file >> ntuples;
                vector<Cost> costs;
                for (a = 0; a < x->getDomainInitSize(); a++) {
                    for (b = 0; b < y->getDomainInitSize(); b++) {
					    Cost tmpcost = defval*K;
						if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
                        costs.push_back(tmpcost);
                    }
                }
            	for (k = 0; k < ntuples; k++) {
                    file >> a;
                    file >> b;
                    file >> cost;
					Cost tmpcost = cost*K;
					if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
                    costs[a * y->getDomainInitSize() + b] = tmpcost;
                }
                
                if(ToulBar2::vac) {
	                for (a = 0; a < x->getDomainInitSize(); a++) {
	                    for (b = 0; b < y->getDomainInitSize(); b++) {
	             			Cost c = costs[a * y->getDomainInitSize() + b];
	                        histogram(c);
	                    }
	                }               	
                }
                if((defval != MIN_COST) || (ntuples > 0)) postBinaryConstraint(i,j,costs);
            } else {
                file >> funcname;
                if (funcname == ">=") {
                    file >> funcparam1;
                    file >> funcparam2;
                    postSupxyc(i,j,funcparam1,funcparam2);
                } else if (funcname == ">") {
                    file >> funcparam1;
                    file >> funcparam2;
                    postSupxyc(i,j,funcparam1 + 1,funcparam2);
                } else if (funcname == "<=") {
                    file >> funcparam1;
                    file >> funcparam2;
                    postSupxyc(j,i, -funcparam1,funcparam2);
                } else if (funcname == "<") {
                    file >> funcparam1;
                    file >> funcparam2;
                    postSupxyc(j,i, -funcparam1 + 1,funcparam2);
                } else if (funcname == "=") {
                    file >> funcparam1;
                    file >> funcparam2;
                    postSupxyc(i,j,funcparam1,funcparam2);
                    postSupxyc(j,i,-funcparam1,funcparam2);
                } else if (funcname == "disj") {
				  Cost funcparam3;
				  file >> funcparam1;
				  file >> funcparam2;
				  file >> funcparam3;
				  postDisjunction(i,j,funcparam1,funcparam2,funcparam3);
                } else if (funcname == "sdisj") {
				  Value funcparam3;
				  Value funcparam4;
				  Cost funcparam5;
				  Cost funcparam6;
				  file >> funcparam1;
				  file >> funcparam2;
				  file >> funcparam3;
				  file >> funcparam4;
				  file >> funcparam5;
				  file >> funcparam6;
				  postSpecialDisjunction(i,j,funcparam1,funcparam2,funcparam3,funcparam4,funcparam5,funcparam6);
                } else {
                    cerr << "Error: function " << funcname << " not implemented!" << endl;
                    exit(EXIT_FAILURE);
                }
            }
        } else if (arity == 1) {
            file >> i;
            if (ToulBar2::verbose >= 3) cout << "read unary constraint " << ic << " on " << i << endl;
			if (vars[i]->enumerated()) {
			  EnumeratedVariable *x = (EnumeratedVariable *) vars[i];
			  file >> defval;
			  file >> ntuples;
			  TemporaryUnaryConstraint unaryconstr;
			  unaryconstr.var = x;
			  for (a = 0; a < x->getDomainInitSize(); a++) {
				Cost tmpcost = defval*K;
				if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
                unaryconstr.costs.push_back(tmpcost);
			  }
			  for (k = 0; k < ntuples; k++) {
                file >> a;
                file >> cost;
				Cost tmpcost = cost*K;
				if (CUT(tmpcost, getUb())) tmpcost *= MEDIUM_COST;
                unaryconstr.costs[a] = tmpcost;
			  }
			  if(ToulBar2::vac) {
                for (a = 0; a < x->getDomainInitSize(); a++) {
				  Cost c = unaryconstr.costs[a];
				  histogram(c);
                }               	
			  }
			  unaryconstrs.push_back(unaryconstr);
			  x->queueNC();
			} else {
			  file >> defval;
			  if (defval == MIN_COST) {
				cerr << "Error: unary cost function with zero penalty cost!" << endl;
				exit(EXIT_FAILURE);
			  }
			  file >> ntuples;
			  Value *dom = new Value[ntuples];
			  for (k = 0; k < ntuples; k++) {
                file >> dom[k];
                file >> cost;
				if (cost != MIN_COST) {
				  cerr << "Error: unary cost function with non-zero cost tuple!" << endl;
				  exit(EXIT_FAILURE);
				}
			  }			  
			  postUnary(i,dom,ntuples,defval);
			  delete [] dom;
			}
        } else if (arity == 0) {
            file >> defval;
            file >> ntuples;
            if (ToulBar2::verbose >= 3) cout << "read global lower bound contribution " << ic << " of " << defval << endl;
        	if (ntuples != 0) {
                cerr << "Error: global lower bound contribution with several tuples!" << endl;
                exit(EXIT_FAILURE);
            }
            inclowerbound += defval*K;
        } 
    }
    
	file >> funcname;
	if (file) {
	  cerr << "Warning: EOF not reached after reading all the constraints (initial number of constraints too small?)" << endl;
	}
    sortVariables();
    sortConstraints();
    // apply basic initial propagation AFTER complete network loading
    increaseLb(getLb() + inclowerbound);
    for (unsigned int u=0; u<unaryconstrs.size(); u++) {
        for (a = 0; a < unaryconstrs[u].var->getDomainInitSize(); a++) {
            if (unaryconstrs[u].costs[a] > MIN_COST) unaryconstrs[u].var->project(a, unaryconstrs[u].costs[a]);
        }
        unaryconstrs[u].var->findSupport();
    }
    if (ToulBar2::verbose >= 0) {
        cout << "Read " << nbvar << " variables, with " << nbval << " values at most, and " << nbconstr << " constraints." << endl;
    }   
    histogram();
    
}

void WCSP::read_random(int n, int m, vector<int>& p, int seed, bool forceSubModular ) 
{
	naryRandom randwcsp(this,seed);
    randwcsp.Input(n,m,p,forceSubModular);	    
 
 	unsigned int nbconstr = numberOfConstraints();
    sortVariables();
    sortConstraints();
    
    if (ToulBar2::verbose >= 0) {
        cout << "Generated random problem " << n << " variables, with " << m << " values, and " << nbconstr << " constraints." << endl;
    }  
}
