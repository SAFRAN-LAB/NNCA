#ifndef _FMM3DTreeRAMeff2_HPP__
#define _FMM3DTreeRAMeff2_HPP__

class FMM3DBox {
public:
	int boxNumber;
	int parentNumber;
	int childrenNumbers[8];
	int neighborNumbers[26];//other than self
	std::vector<int> interactionList;

	FMM3DBox () {
		boxNumber		=	-1;
		parentNumber	=	-1;
		for (int l=0; l<8; ++l) {
			childrenNumbers[l]	=	-1;
		}
		for (int l=0; l<26; ++l) {
			neighborNumbers[l]	=	-1;
		}
	}

	pts3D center;
	Eigen::VectorXd charges, potential;
	// std::map<int, Eigen::MatrixXd> U, Vt, fullBlocks;
	Eigen::MatrixXd L[216], R[216]; //upper bound of neighbors for FMM3D is 27; upper bound of IL for FMM3D is 216=27*8;
	std::vector<int> row_basis[216], col_basis[216];
	// Eigen::MatrixXd U[216], Vt[216], fullBlocks[27]; //upper bound of neighbors for FMM3D is 27; upper bound of IL for FMM3D is 216=27*8;
	// std::vector<Eigen::MatrixXd> U;
	// std::vector<Eigen::MatrixXd> Vt;
  std::vector<int> chargeLocations;
	Eigen::VectorXd outgoing_charges;//equivalent densities {f_{k}^{B,o}}
	Eigen::VectorXd incoming_charges;//equivalent densities {f_{k}^{B,i}}
	Eigen::VectorXd incoming_potential;//check potentials {u_{k}^{B,i}}
	std::vector<int> incoming_chargePoints;//equivalent points {y_{k}^{B,i}}
	std::vector<int> incoming_checkPoints;//check points {x_{k}^{B,i}}
	std::map<int, Eigen::MatrixXd> M2L;
	Eigen::MatrixXd L2P;					//	Transfer from multipoles of 4 children to multipoles of parent.
};

template <typename kerneltype>
class FMM3DTree {
public:
	kerneltype* K;
	int nLevels;			//	Number of levels in the tree.
	int N;					//	Number of particles.
	int cubeRootN;					//	Number of particles.
	double L;				//	Semi-length of the simulation box.
	double smallestBoxSize;	//	This is L/2.0^(nLevels).

	std::vector<int> nBoxesPerLevel;			//	Number of boxes at each level in the tree.
	std::vector<double> boxRadius;				//	Box radius at each level in the tree assuming the box at the root is [-1,1]^2
	std::vector<std::vector<FMM3DBox> > tree;	//	The tree storing all the information.

	double ACA_epsilon;
	int nParticlesInLeafAlong1D;
  int nParticlesInLeaf;
  std::vector<double> Nodes1D;
	std::vector<pts3D> Nodes;
  std::vector<pts3D> gridPoints; //all particles in domain
	int TOL_POW;
	double assTime, matVecTime;

// public:
FMM3DTree(kerneltype* K, int cubeRootN, int nLevels, int nParticlesInLeafAlong1D, double L, int TOL_POW) {
	this->K					=	K;
	this->cubeRootN	=	cubeRootN;
	this->nLevels		=	nLevels;
		this->L					=	L;
    this->nParticlesInLeafAlong1D = nParticlesInLeafAlong1D;
    this->nParticlesInLeaf = nParticlesInLeafAlong1D*nParticlesInLeafAlong1D*nParticlesInLeafAlong1D;
		this->TOL_POW = TOL_POW;
    nBoxesPerLevel.push_back(1);
		boxRadius.push_back(L);
		for (int k=1; k<=nLevels; ++k) {
			nBoxesPerLevel.push_back(8*nBoxesPerLevel[k-1]);
			boxRadius.push_back(0.5*boxRadius[k-1]);
		}
		this->smallestBoxSize	=	boxRadius[nLevels];
		K->a					=	smallestBoxSize;
		this->N					=	cubeRootN*cubeRootN*cubeRootN;
		this->assTime = 0.0;
		this->matVecTime = 0.0;

	}

  void set_Standard_Cheb_Nodes() {
		for (int k=0; k<cubeRootN; ++k) {
			Nodes1D.push_back(-cos((k+0.5)/cubeRootN*PI));
		}
		pts3D temp1;
		for (int j=0; j<cubeRootN; ++j) {
			for (int k=0; k<cubeRootN; ++k) {
				for (int i=0; i<cubeRootN; ++i) {
					temp1.x	=	Nodes1D[k];
					temp1.y	=	Nodes1D[j];
					temp1.z	=	Nodes1D[i];
					K->particles.push_back(temp1);
				}
			}
		}
	}

	void set_Uniform_Nodes(double h) {
		double loc = -L+h/2.0;
		for (int k=0; k<cubeRootN; ++k) {
			Nodes1D.push_back(loc);
			loc += h;
		}
		pts3D temp1;
		for (int j=0; j<cubeRootN; ++j) {
			for (int k=0; k<cubeRootN; ++k) {
				for (int i=0; i<cubeRootN; ++i) {
					temp1.x	=	Nodes1D[k];
					temp1.y	=	Nodes1D[j];
					temp1.z	=	Nodes1D[i];
					K->particles.push_back(temp1);
				}
			}
		}
	}

	// void set_Uniform_Nodes() {
	// 	for (int k=0; k<nParticlesInLeafAlong1D; ++k) {
	// 		Nodes1D.push_back(-1.0+2.0*(k+1.0)/(nParticlesInLeafAlong1D+1.0));
	// 	}
	// 	pts3D temp1;
	// 	for (int j=0; j<nParticlesInLeafAlong1D; ++j) {
	// 		for (int k=0; k<nParticlesInLeafAlong1D; ++k) {
	// 			for (int i=0; i<nParticlesInLeafAlong1D; ++i) {
	// 				temp1.x	=	Nodes1D[k];
	// 				temp1.y	=	Nodes1D[j];
	// 				temp1.z	=	Nodes1D[i];
	// 				Nodes.push_back(temp1);
	// 			}
	// 		}
	// 	}
	// }

	// void set_Uniform_Nodes() {
	// 	for (int k=0; k<cubeRootN; ++k) {
	// 		Nodes1D.push_back(-L+2.0*L*(k+1.0)/(cubeRootN+1.0));
	// 	}
	// 	pts3D temp1;
	// 	for (int j=0; j<cubeRootN; ++j) {
	// 		for (int k=0; k<cubeRootN; ++k) {
	// 			for (int i=0; i<cubeRootN; ++i) {
	// 				temp1.x	=	Nodes1D[k];
	// 				temp1.y	=	Nodes1D[j];
	// 				temp1.z	=	Nodes1D[i];
	// 				K->particles.push_back(temp1);
	// 			}
	// 		}
	// 	}
	// }

  void shift_Nodes(double radius, pts3D center, std::vector<pts3D> &particle_loc) {
    for (size_t i = 0; i < Nodes.size(); i++) {
      pts3D temp;
      temp.x = Nodes[i].x*radius + center.x;
      temp.y = Nodes[i].y*radius + center.y;
			temp.z = Nodes[i].z*radius + center.z;
      particle_loc.push_back(temp);
    }
  }

	void createTree() {
		//	First create root and add to tree
		FMM3DBox root;
		root.boxNumber		=	0;
		root.parentNumber	=	-1;
		#pragma omp parallel for
		for (int l=0; l<8; ++l) {
			root.childrenNumbers[l]	=	l;
		}
		#pragma omp parallel for
		for (int l=0; l<26; ++l) {
			root.neighborNumbers[l]	=	-1;
		}
		std::vector<FMM3DBox> rootLevel;
		rootLevel.push_back(root);
		tree.push_back(rootLevel);

		for (int j=1; j<=nLevels; ++j) {
			std::vector<FMM3DBox> level;
			for (int k=0; k<nBoxesPerLevel[j]; ++k) {
				FMM3DBox box;
				box.boxNumber		=	k;
				box.parentNumber	=	k/8;
				for (int l=0; l<8; ++l) {
					box.childrenNumbers[l]	=	8*k+l;
				}
				level.push_back(box);
			}
			tree.push_back(level);
		}
	}

	void check() {
		for (size_t j = 2; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				std::cout << "j: " << j << "	k: " << k << "	IL: " << tree[j][k].interactionList.size() << std::endl;
			}
		}
	}

	void check2() {
		int j=1;
		for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
			std::cout << "j: " << j << "	k: " << k << std::endl;
			for (size_t n = 0; n < 26; n++) {
				if (tree[j][k].neighborNumbers[n] != -1) {
					std::cout << tree[j][k].neighborNumbers[n] << "," << std::endl;
				}
			}
			std::cout << std::endl;
		}
	}

	//	Assigns the interactions for child0 of a box
	void assign_Child0_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[13]	=	8*k+1;
			tree[nL][nC].neighborNumbers[16]	=	8*k+2;
			tree[nL][nC].neighborNumbers[15]	=	8*k+3;
			tree[nL][nC].neighborNumbers[21]	=	8*k+4;
			tree[nL][nC].neighborNumbers[22]	=	8*k+5;
			tree[nL][nC].neighborNumbers[25]	=	8*k+6;
			tree[nL][nC].neighborNumbers[24]	=	8*k+7;
		}

		//	Assign children of parent's zeroth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[0];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's first neighbor
		{
			nN	=	tree[j][k].neighborNumbers[1];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's second neighbor
		{
			nN	=	tree[j][k].neighborNumbers[2];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's third neighbor
		{
			nN	=	tree[j][k].neighborNumbers[3];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's fourth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[4];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[4]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
			}
		}

		//	Assign children of parent's fifth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[5];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's sixth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[6];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's seventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[7];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's eigth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[8];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's ninth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[9];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[9]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[17]=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's tenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[10];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[10] =	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[19] =	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[18] =	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's eleventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[11];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's twelveth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[12]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[14] =	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[20] =	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[23] =	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 13; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child1 of a box
	void assign_Child1_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+1;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[12]	=	8*k+0;
			tree[nL][nC].neighborNumbers[15]	=	8*k+2;
			tree[nL][nC].neighborNumbers[14]	=	8*k+3;
			tree[nL][nC].neighborNumbers[20]	=	8*k+4;
			tree[nL][nC].neighborNumbers[21]	=	8*k+5;
			tree[nL][nC].neighborNumbers[24]	=	8*k+6;
			tree[nL][nC].neighborNumbers[23]	=	8*k+7;
		}

		//	Assign children of parent's zeroth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[0];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's first neighbor
		{
			nN	=	tree[j][k].neighborNumbers[1];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's second neighbor
		{
			nN	=	tree[j][k].neighborNumbers[2];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's third neighbor
		{
			nN	=	tree[j][k].neighborNumbers[3];
			if (nN!=-1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's fourth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[4];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[4]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
			}
		}

		//	Assign children of parent's fifth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[5];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's sixth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[6];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's seventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[7];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's eigth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[8];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's ninth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[9];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's tenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[10];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[10]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[9]	  =	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[18]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's eleventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[11];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's twelveth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's thirteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[13]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[16]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		for (size_t n = 14; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child2 of a box
	void assign_Child2_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+2;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[9]	=	8*k+0;
			tree[nL][nC].neighborNumbers[10]	=	8*k+1;
			tree[nL][nC].neighborNumbers[12]	=	8*k+3;
			tree[nL][nC].neighborNumbers[17]	=	8*k+4;
			tree[nL][nC].neighborNumbers[18]	=	8*k+5;
			tree[nL][nC].neighborNumbers[21]	=	8*k+6;
			tree[nL][nC].neighborNumbers[20]	=	8*k+7;
		}

		//	Assign children of parent's zeroth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[0];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's first neighbor
		{
			nN	=	tree[j][k].neighborNumbers[1];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's second neighbor
		{
			nN	=	tree[j][k].neighborNumbers[2];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's third neighbor
		{
			nN	=	tree[j][k].neighborNumbers[3];
			if (nN!=-1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's fourth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[4];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[4]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
			}
		}

		//	Assign children of parent's fifth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[5];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's sixth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[6];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's seventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[7];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's eigth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[8];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's ninth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[9];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's tenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[10];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's eleventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[11];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's twelveth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's thirteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[13]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's fourteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[14];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's fifteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[15];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[14]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[15]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's sixteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[16];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[16]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 17; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child3 of a box
	void assign_Child3_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+3;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[10]	=	8*k+0;
			tree[nL][nC].neighborNumbers[11]	=	8*k+1;
			tree[nL][nC].neighborNumbers[13]	=	8*k+2;
			tree[nL][nC].neighborNumbers[18]	=	8*k+4;
			tree[nL][nC].neighborNumbers[19]	=	8*k+5;
			tree[nL][nC].neighborNumbers[22]	=	8*k+6;
			tree[nL][nC].neighborNumbers[21]	=	8*k+7;
		}

		for (size_t n = 0; n <= 2; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's third neighbor
		{
			nN	=	tree[j][k].neighborNumbers[3];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's fourth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[4];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[4]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
			}
		}

		//	Assign children of parent's fifth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[5];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's sixth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[6];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's seventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[7];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 8; n <= 11; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's twelveth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[9]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[12]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[20]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's thirteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's fourteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[14];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[14]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's fifteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[15];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[15]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[16]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 16; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child4 of a box
	void assign_Child4_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+4;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[4]	=	8*k+0;
			tree[nL][nC].neighborNumbers[5]	=	8*k+1;
			tree[nL][nC].neighborNumbers[8]	=	8*k+2;
			tree[nL][nC].neighborNumbers[7]	=	8*k+3;
			tree[nL][nC].neighborNumbers[13]	=	8*k+5;
			tree[nL][nC].neighborNumbers[16]	=	8*k+6;
			tree[nL][nC].neighborNumbers[15]	=	8*k+7;
		}

		for (size_t n = 0; n <= 8; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's nineth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[9];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[9]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's tenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[10];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[10]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's eleventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[11];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's twelveth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[12]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[14]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 13; n <= 16; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's seventeenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[17];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's eighteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[18];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[18]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's nineteenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[19];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's twentieth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[20];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[20]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's twentyfirst neighbor
		{
			nN	=	tree[j][k].neighborNumbers[21];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[21]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}


		for (size_t n = 22; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child5 of a box
	void assign_Child5_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+5;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[3]	=	8*k+0;
			tree[nL][nC].neighborNumbers[4]	=	8*k+1;
			tree[nL][nC].neighborNumbers[7]	=	8*k+2;
			tree[nL][nC].neighborNumbers[6]	=	8*k+3;
			tree[nL][nC].neighborNumbers[12]	=	8*k+4;
			tree[nL][nC].neighborNumbers[15]	=	8*k+6;
			tree[nL][nC].neighborNumbers[14]	=	8*k+7;
		}

		for (size_t n = 0; n <= 8; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's nineth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[9];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's tenth neighbor
		{
			nN	=	tree[j][k].neighborNumbers[10];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[1]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[10]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].neighborNumbers[9]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
			}
		}

		//	Assign children of parent's eleventh neighbor
		{
			nN	=	tree[j][k].neighborNumbers[11];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's 12th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 13 neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[13]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[16]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		for (size_t n = 14; n <= 17; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's 18th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[18];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[18]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 19th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[19];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 20th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[20];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 21st neighbor
		{
			nN	=	tree[j][k].neighborNumbers[21];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[20]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[21]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 22nd neighbor
		{
			nN	=	tree[j][k].neighborNumbers[22];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 23; n <= 25; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

	}

	//	Assigns the interactions for child6 of a box
	void assign_Child6_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+6;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[0]	=	8*k+0;
			tree[nL][nC].neighborNumbers[1]	=	8*k+1;
			tree[nL][nC].neighborNumbers[4]	=	8*k+2;
			tree[nL][nC].neighborNumbers[3]	=	8*k+3;
			tree[nL][nC].neighborNumbers[9]	=	8*k+4;
			tree[nL][nC].neighborNumbers[10]	=	8*k+5;
			tree[nL][nC].neighborNumbers[12]	=	8*k+7;
		}

		for (size_t n = 0; n <= 12; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's 13th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[2]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[5]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].neighborNumbers[11]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[13]	=	tree[j][nN].childrenNumbers[7];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
			}
		}

		//	Assign children of parent's 14th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[14];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 15th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[15];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[14]=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[15]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 16th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[16];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[8]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[16]	=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 17; n <= 20; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's 21st neighbor
		{
			nN	=	tree[j][k].neighborNumbers[21];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[18]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[21]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[20]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 22nd neighbor
		{
			nN	=	tree[j][k].neighborNumbers[22];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 23rd neighbor
		{
			nN	=	tree[j][k].neighborNumbers[23];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 24th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[24];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 25th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[25];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[1]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

	}

	//	Assigns the interactions for child7 of a box
	void assign_Child7_Interaction(int j, int k) {
		int nL	=	j+1;
		int nC	=	8*k+7;
		int nN;

		//	Assign siblings
		{
			tree[nL][nC].neighborNumbers[1]	=	8*k+0;
			tree[nL][nC].neighborNumbers[2]	=	8*k+1;
			tree[nL][nC].neighborNumbers[5]	=	8*k+2;
			tree[nL][nC].neighborNumbers[4]	=	8*k+3;
			tree[nL][nC].neighborNumbers[10]=	8*k+4;
			tree[nL][nC].neighborNumbers[11]=	8*k+5;
			tree[nL][nC].neighborNumbers[13]=	8*k+6;
		}

		for (size_t n = 0; n <= 11; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's 12th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[12];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[0]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[3]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[9]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].neighborNumbers[12]	=	tree[j][nN].childrenNumbers[6];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 13th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[13];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 14th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[14];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[6]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[14]	=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 15th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[15];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[7]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[8] =	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[15]=	tree[j][nN].childrenNumbers[4];
				tree[nL][nC].neighborNumbers[16]=	tree[j][nN].childrenNumbers[5];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		for (size_t n = 16; n <= 19; n++) {
			//	Assign children of parent's nth neighbor
			{
				nN	=	tree[j][k].neighborNumbers[n];
				if (nN != -1) {
					for (size_t i = 0; i < 8; i++) {
						tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
					}
				}
			}
		}

		//	Assign children of parent's 20th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[20];
			if (nN!=-1) {
				tree[nL][nC].neighborNumbers[17]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[20]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 21st neighbor
		{
			nN	=	tree[j][k].neighborNumbers[21];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[18]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[19]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].neighborNumbers[22]	=	tree[j][nN].childrenNumbers[2];
				tree[nL][nC].neighborNumbers[21]	=	tree[j][nN].childrenNumbers[3];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 22nd neighbor
		{
			nN	=	tree[j][k].neighborNumbers[22];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

		//	Assign children of parent's 23rd neighbor
		{
			nN	=	tree[j][k].neighborNumbers[23];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[23]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[0]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 24th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[24];
			if (nN != -1) {
				tree[nL][nC].neighborNumbers[24]	=	tree[j][nN].childrenNumbers[0];
				tree[nL][nC].neighborNumbers[25]	=	tree[j][nN].childrenNumbers[1];
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[2]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[3]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[4]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[5]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[6]);
				tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[7]);
			}
		}

		//	Assign children of parent's 25th neighbor
		{
			nN	=	tree[j][k].neighborNumbers[25];
			if (nN != -1) {
				for (size_t i = 0; i < 8; i++) {
					tree[nL][nC].interactionList.push_back(tree[j][nN].childrenNumbers[i]);
				}
			}
		}

	}

	//	Assigns the interactions for the children of a box
	void assign_Box_Interactions(int j, int k) {
		assign_Child0_Interaction(j,k);
		assign_Child1_Interaction(j,k);
		assign_Child2_Interaction(j,k);
		assign_Child3_Interaction(j,k);
		assign_Child4_Interaction(j,k);
		assign_Child5_Interaction(j,k);
		assign_Child6_Interaction(j,k);
		assign_Child7_Interaction(j,k);
	}

	//	Assigns the interactions for the children all boxes at a given level
	void assign_Level_Interactions(int j) {
		#pragma omp parallel for
		for (int k=0; k<nBoxesPerLevel[j]; ++k) {
			assign_Box_Interactions(j,k);
		}
	}

	//	Assigns the interactions for the children all boxes in the tree
	void assign_Tree_Interactions() {
		for (int j=0; j<nLevels; ++j) {
			assign_Level_Interactions(j);
		}
	}

	void assign_Center_Location() {
		int J;
		tree[0][0].center.x	=	0.0;
		tree[0][0].center.y	=	0.0;
		tree[0][0].center.z	=	0.0;
		for (int j=0; j<nLevels; ++j) {
			J	=	j+1;
			double shift	=	0.5*boxRadius[j];
			#pragma omp parallel for
			for (int k=0; k<nBoxesPerLevel[j]; ++k) {
				tree[J][8*k].center.x		=	tree[j][k].center.x-shift;
				tree[J][8*k+1].center.x	=	tree[j][k].center.x+shift;
				tree[J][8*k+2].center.x	=	tree[j][k].center.x+shift;
				tree[J][8*k+3].center.x	=	tree[j][k].center.x-shift;
				tree[J][8*k+4].center.x	=	tree[j][k].center.x-shift;
				tree[J][8*k+5].center.x	=	tree[j][k].center.x+shift;
				tree[J][8*k+6].center.x	=	tree[j][k].center.x+shift;
				tree[J][8*k+7].center.x	=	tree[j][k].center.x-shift;

				tree[J][8*k].center.y		=	tree[j][k].center.y-shift;
				tree[J][8*k+1].center.y	=	tree[j][k].center.y-shift;
				tree[J][8*k+2].center.y	=	tree[j][k].center.y+shift;
				tree[J][8*k+3].center.y	=	tree[j][k].center.y+shift;
				tree[J][8*k+4].center.y	=	tree[j][k].center.y-shift;
				tree[J][8*k+5].center.y	=	tree[j][k].center.y-shift;
				tree[J][8*k+6].center.y	=	tree[j][k].center.y+shift;
				tree[J][8*k+7].center.y	=	tree[j][k].center.y+shift;

				tree[J][8*k].center.z		=	tree[j][k].center.z-shift;
				tree[J][8*k+1].center.z	=	tree[j][k].center.z-shift;
				tree[J][8*k+2].center.z	=	tree[j][k].center.z-shift;
				tree[J][8*k+3].center.z	=	tree[j][k].center.z-shift;
				tree[J][8*k+4].center.z	=	tree[j][k].center.z+shift;
				tree[J][8*k+5].center.z	=	tree[j][k].center.z+shift;
				tree[J][8*k+6].center.z	=	tree[j][k].center.z+shift;
				tree[J][8*k+7].center.z	=	tree[j][k].center.z+shift;
			}
		}
	}

	void assignChargeLocations() {
		// K->particles = Nodes;//object of base class FMM_Matrix
		for (size_t i = 0; i < N; i++) {
			tree[0][0].chargeLocations.push_back(i);
		}
		for (size_t j = 0; j < nLevels; j++) { //assign particles to its children
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				int J = j+1;
				int Kp = 8*k;
				for (size_t i = 0; i < tree[j][k].chargeLocations.size(); i++) {
					int index = tree[j][k].chargeLocations[i];
					if (K->particles[index].z <= tree[j][k].center.z) { //children 0,1,2,3
						if (K->particles[index].x <= tree[j][k].center.x) { //children 0,3
							if (K->particles[index].y <= tree[j][k].center.y) { //child 0
								tree[J][Kp].chargeLocations.push_back(index);
							}
							else { //child 3
								tree[J][Kp+3].chargeLocations.push_back(index);
							}
						}
						else { //children 1,2
							if (K->particles[index].y <= tree[j][k].center.y) { //child 1
								tree[J][Kp+1].chargeLocations.push_back(index);
							}
							else { //child 2
								tree[J][Kp+2].chargeLocations.push_back(index);
							}
						}
					}
					else {//children 4,5,6,7
						if (K->particles[index].x <= tree[j][k].center.x) { //children 4,7
							if (K->particles[index].y <= tree[j][k].center.y) { //child 4
								tree[J][Kp+4].chargeLocations.push_back(index);
							}
							else { //child 7
								tree[J][Kp+7].chargeLocations.push_back(index);
							}
						}
						else { //children 5,6
							if (K->particles[index].y <= tree[j][k].center.y) { //child 5
								tree[J][Kp+5].chargeLocations.push_back(index);
							}
							else { //child 6
								tree[J][Kp+6].chargeLocations.push_back(index);
							}
						}
					}
				}
			}
		}
	}

	void assignCharges(Eigen::VectorXd &charges) {
		int start = 0;
		// std::cout << "charges" << std::endl;
		for (size_t j = 1; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				tree[j][k].charges = Eigen::VectorXd::Zero(tree[j][k].chargeLocations.size());
				for (size_t i = 0; i < tree[j][k].chargeLocations.size(); i++) {
					tree[j][k].charges[i] = charges[tree[j][k].chargeLocations[i]];
					// std::cout << tree[j][k].chargeLocations[i] << std::endl;
				}
				// std::cout << "---------" << std::endl;
			}
		}
	}

  // void assignLeafChargeLocations() {
	// 	for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
  //     int startIndex = gridPoints.size();
  //     for (size_t i = 0; i < nParticlesInLeaf; i++) {
  //       tree[nLevels][k].chargeLocations.push_back(startIndex+i);
  //     }
  //     shift_Nodes(boxRadius[nLevels], tree[nLevels][k].center, gridPoints);
  //   }
  //   K->particles_X = gridPoints;//object of base class FMM_Matrix
  //   K->particles_Y = gridPoints;
  // }

	void assignNonLeafChargeLocations() {
		for (int j = nLevels-1; j >= 1; j--) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				tree[j][k].chargeLocations.clear();
				for (size_t c = 0; c < 8; c++) {
					tree[j][k].chargeLocations.insert(tree[j][k].chargeLocations.end(), tree[j+1][8*k+c].chargeLocations.begin(), tree[j+1][8*k+c].chargeLocations.end());
				}
			}
		}
	}


	void getNodes() {
		for (int j=nLevels; j>=2; j--) {
			getNodes_incoming_level(j);
		}
	}

	void getParticlesFromChildren_incoming(int j, int k, std::vector<int>& searchNodes) {
		if (j==nLevels) {
			searchNodes.insert(searchNodes.end(), tree[j][k].chargeLocations.begin(), tree[j][k].chargeLocations.end());
		}
		else {
			int J = j+1;
			for (int c = 0; c < 8; c++) {
				searchNodes.insert(searchNodes.end(), tree[J][8*k+c].incoming_checkPoints.begin(), tree[J][8*k+c].incoming_checkPoints.end());
			}
		}
	}

	void getNodes_incoming_box(int j, int k, int& ComputedRank) {
		std::vector<int> boxA_Nodes;
		getParticlesFromChildren_incoming(j, k, boxA_Nodes);
		std::vector<int> IL_Nodes;//indices
		for(int in=0; in<tree[j][k].interactionList.size(); ++in) {
			int kIL = tree[j][k].interactionList[in];
			std::vector<int> chargeLocations;
			getParticlesFromChildren_incoming(j, kIL, chargeLocations);
			IL_Nodes.insert(IL_Nodes.end(), chargeLocations.begin(), chargeLocations.end());
		}
		// for(int on=0; on<24; ++on) {
		// 	if(tree[j][k].outerNumbers[on] != -1) {
		// 		int kIL = tree[j][k].outerNumbers[on];
		// 		std::vector<int> chargeLocations;
		// 		getParticlesFromChildren_incoming(j, kIL, chargeLocations);
		// 		IL_Nodes.insert(IL_Nodes.end(), chargeLocations.begin(), chargeLocations.end());
		// 	}
		// }
		std::vector<int> row_bases, col_bases;
		Eigen::MatrixXd Ac, Ar, L, R;
		LowRank* LR		=	new LowRank(K, TOL_POW, boxA_Nodes, IL_Nodes);
		LR->ACA_only_nodes(row_bases, col_bases, ComputedRank, Ac, Ar, L, R);
		// LR->ACA_only_nodes(row_bases, col_bases, ComputedRank, Ac, Ar);
		if(ComputedRank > 0) {
			for (int r = 0; r < row_bases.size(); r++) {
				tree[j][k].incoming_checkPoints.push_back(boxA_Nodes[row_bases[r]]);
			}
			for (int c = 0; c < col_bases.size(); c++) {
				tree[j][k].incoming_chargePoints.push_back(IL_Nodes[col_bases[c]]);
			}
			// Eigen::MatrixXd D = K->getMatrix(tree[j][k].incoming_checkPoints, tree[j][k].incoming_chargePoints);
			// Eigen::PartialPivLU<Eigen::MatrixXd> D_T = Eigen::PartialPivLU<Eigen::MatrixXd>(D.transpose());
			// Eigen::PartialPivLU<Eigen::MatrixXd> D_T_T = Eigen::PartialPivLU<Eigen::MatrixXd>(D);
			// tree[j][k].L2P = D_T.solve(Ac.transpose()).transpose();

			Eigen::MatrixXd temp = R.triangularView<Eigen::Upper>().solve<Eigen::OnTheRight>(Ac);
			tree[j][k].L2P = L.triangularView<Eigen::Lower>().solve<Eigen::OnTheRight>(temp);
			// Eigen::MatrixXd Error_LR_D = L*R - D;
			// std::cout << "E_LR_D: " << Error_LR_D.norm()/D.norm() << std::endl;
			// std::cout << "mat: " << std::endl << K->getMatrix(boxA_Nodes, IL_Nodes) << std::endl;
			// Eigen::MatrixXd Error_AcAr_LR = K->getMatrix(boxA_Nodes, IL_Nodes) - Ac*D_T_T.solve(Ar);//tree[j][k].L2P*Ar;//K->getMatrix(boxA_Nodes, IL_Nodes);
			// std::cout << "E: " << Error_AcAr_LR.norm() << std::endl;
			// std::cout << "ComputedRank: " << ComputedRank << std::endl;
			//
			// std::cout << "L: " << std::endl << L << std::endl;
			// std::cout << "R: " << std::endl << R << std::endl;
			// std::cout << "LR: " << std::endl << L*R << std::endl;
			// std::cout << "D: " << std::endl << D << std::endl;
			// std::cout << "LR-D: " << std::endl << L*R-D << std::endl;
			// exit(0);

		}
	}

	void getNodes_incoming_level(int j) {
		int ComputedRank;
		for (int k=0; k<nBoxesPerLevel[j]; ++k) {
			getNodes_incoming_box(j, k, ComputedRank);
		}
	}

	void assemble_M2L() {
		// #pragma omp parallel for
		for (size_t j = 2; j <= nLevels; j++) {
			#pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				// #pragma omp parallel for
				for(int in=0; in<tree[j][k].interactionList.size(); ++in) {
					int kIL = tree[j][k].interactionList[in];
					if (tree[j][k].M2L[kIL].size() == 0)
						tree[j][k].M2L[kIL] = K->getMatrix(tree[j][k].incoming_checkPoints, tree[j][kIL].incoming_checkPoints);
					if (tree[j][kIL].M2L[k].size() == 0)
						tree[j][kIL].M2L[k] = tree[j][k].M2L[kIL].transpose();
				}
				// #pragma omp parallel for
				// for(int on=0; on<24; ++on) {
				// 	if(tree[j][k].outerNumbers[on] != -1) {
				// 		int kIL = tree[j][k].outerNumbers[on];
				// 		if (tree[j][k].M2L[kIL].size() == 0)
				// 			tree[j][k].M2L[kIL] = K->getMatrix(tree[j][k].incoming_checkPoints, tree[j][kIL].incoming_checkPoints);
				// 		if (tree[j][kIL].M2L[k].size() == 0)
				// 			tree[j][kIL].M2L[k] = tree[j][k].M2L[kIL].transpose();
				// 	}
				// }
			}
		}
	}


	void evaluate_M2M() {
		for (size_t j = nLevels; j >= 2; j--) {
			#pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				Eigen::VectorXd source_densities;
				if (j==nLevels) {
					source_densities = tree[j][k].charges;
				}
				else {
					int J = j+1;
					int Veclength = 0;
					for (int child = 0; child < 8; child++) {
						Veclength += tree[J][8*k+child].outgoing_charges.size();
					}
					source_densities = Eigen::VectorXd::Zero(Veclength);// = tree[j][k].multipoles//source densities
					int start = 0;
					for (int child = 0; child < 8; child++) {
						int NumElem = tree[J][8*k+child].outgoing_charges.size();
						source_densities.segment(start, NumElem) = tree[J][8*k+child].outgoing_charges;
						start += NumElem;
					}
				}
				tree[j][k].outgoing_charges = tree[j][k].L2P.transpose()*source_densities;//f^{B,o} //solve system: A\tree[j][k].outgoing_potential
			}
		}
	}

	void evaluate_M2L() {
		// #pragma omp parallel for
		for (int j=2; j<=nLevels; ++j) {
			#pragma omp parallel for
			for (int k=0; k<nBoxesPerLevel[j]; ++k) {//BoxA
				tree[j][k].incoming_potential	=	Eigen::VectorXd::Zero(tree[j][k].incoming_checkPoints.size());
				// #pragma omp parallel for
				for(int in=0; in<tree[j][k].interactionList.size(); ++in) {
					int kIL = tree[j][k].interactionList[in];
					tree[j][k].incoming_potential += tree[j][k].M2L[kIL]*tree[j][kIL].outgoing_charges;
				}
				// #pragma omp parallel for
				// for(int on=0; on<24; ++on) {
				// 	if(tree[j][k].outerNumbers[on] != -1) {
				// 		int kIL = tree[j][k].outerNumbers[on];
				// 		tree[j][k].incoming_potential += tree[j][k].M2L[kIL]*tree[j][kIL].outgoing_charges;
				// 	}
				// }
			}
		}
	}

	void evaluate_L2L() {
		for (size_t j = 2; j <= nLevels; j++) {
			#pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				if (j != nLevels) {
					Eigen::VectorXd temp = tree[j][k].L2P*tree[j][k].incoming_potential;
					int start = 0;
					for (size_t c = 0; c < 8; c++) {
						tree[j+1][8*k+c].incoming_potential += temp.segment(start, tree[j+1][8*k+c].incoming_checkPoints.size());
						start += tree[j+1][8*k+c].incoming_checkPoints.size();
					}
				}
				else {
					tree[j][k].potential = tree[j][k].L2P*tree[j][k].incoming_potential;
				}
			}
		}
	}

	void getUVtTree() {
		// #pragma omp parallel for
		for (size_t j = 2; j <= nLevels; j++) {
			#pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				// #pragma omp parallel for
				for (size_t i = 0; i < tree[j][k].interactionList.size(); i++) {
					int ki = tree[j][k].interactionList[i];
					getUVtInstance(j,k,ki,tree[j][k].L[i],tree[j][k].R[i], tree[j][k].row_basis[i],tree[j][k].col_basis[i]);
				}
			}
		}
	}

	void getUVtInstance(int j, int k, int ki, Eigen::MatrixXd& L,Eigen::MatrixXd& R, std::vector<int>& row_indices, std::vector<int>& col_indices) {
		int computed_rank;
		std::vector<int> row_indices_local,col_indices_local;
		LowRank* LR		=	new LowRank(K, TOL_POW, tree[j][k].chargeLocations, tree[j][ki].chargeLocations);
		LR->ACA_only_nodesCUR(row_indices_local, col_indices_local, computed_rank, L, R);
		for (int r = 0; r < computed_rank; r++) {
			row_indices.push_back(tree[j][k].chargeLocations[row_indices_local[r]]);
		}
		for (int c = 0; c < computed_rank; c++) {
			col_indices.push_back(tree[j][ki].chargeLocations[col_indices_local[c]]);
		}
		delete LR;
	}

	void assignLeafCharges(Eigen::VectorXd &charges) {
		int start = 0;
		for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
			tree[nLevels][k].charges	=	charges.segment(start, nParticlesInLeaf);
			start += nParticlesInLeaf;
		}
	}

	void assignNonLeafCharges() {
		for (int j = nLevels-1; j >= 2; j--) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				int sum = 0;
				for (size_t c = 0; c < 8; c++) {
					sum += tree[j+1][8*k+c].charges.size();
				}
				tree[j][k].charges	=	Eigen::VectorXd(sum);
				int start = 0;
				for (size_t c = 0; c < 8; c++) {
					tree[j][k].charges.segment(start, tree[j+1][8*k+c].charges.size()) = tree[j+1][8*k+c].charges;
					start += tree[j+1][8*k+c].charges.size();
				}
				// std::cout << "tree[j][k].charges: " << tree[j][k].charges.size() << std::endl;
			}
		}
	}

	void evaluateFarField() {
		for (size_t j = 2; j <= nLevels; j++) {
			double assTimeLevel = 0.0;
			double matVecTimeLevel = 0.0;

			#pragma omp parallel
			{
				double start, end;
				double assTimeThread = 0.0;
				double matVecTimeThread = 0.0;

				#pragma omp for nowait
				for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
					tree[j][k].potential = Eigen::VectorXd::Zero(tree[j][k].charges.size());
					for (size_t i = 0; i < tree[j][k].interactionList.size(); i++) {
						if (tree[j][k].col_basis[i].size() > 0) {
							int ki = tree[j][k].interactionList[i];
							start = omp_get_wtime();

							// Eigen::MatrixXd R = K->getMatrix(tree[j][k].chargeLocations,tree[j][ki].chargeLocations);
							// tree[j][k].potential += R*tree[j][ki].charges;

							Eigen::MatrixXd Ac = K->getMatrix(tree[j][k].chargeLocations,tree[j][k].col_basis[i]);
							Eigen::MatrixXd Ar = K->getMatrix(tree[j][k].row_basis[i],tree[j][ki].chargeLocations);
							end = omp_get_wtime();
							assTimeThread += end-start;

							start = omp_get_wtime();
							Eigen::VectorXd t0 = Ar*tree[j][ki].charges;
							Eigen::VectorXd t1 = tree[j][k].L[i].triangularView<Eigen::Lower>().solve(t0);
							Eigen::VectorXd t2 = tree[j][k].R[i].triangularView<Eigen::Upper>().solve(t1);
							tree[j][k].potential += Ac*t2;
							end = omp_get_wtime();
							matVecTimeThread += end-start;
						}
					}
				}
				#pragma omp critical
				{
					if (matVecTimeLevel < matVecTimeThread) {
						matVecTimeLevel = matVecTimeThread;
					}
					if (assTimeLevel < assTimeThread) {
						assTimeLevel = assTimeThread;
					}
				}
			}
			assTime += assTimeLevel;
			matVecTime += matVecTimeLevel;
		}
	}

	int getAvgRank() {
		int sum = 0;
		int NumBoxes = 0;
		// #pragma omp parallel for
		for (size_t j = 2; j <= nLevels; j++) {
			// NumBoxes += nBoxesPerLevel[j];
			// #pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				NumBoxes += 1;
				sum += tree[j][k].incoming_checkPoints.size();
			}
		}
		return sum/NumBoxes;
	}

	int getMaxRank() {
		int maxRank = 0;
		for (size_t j = 2; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				for (size_t i = 0; i < tree[j][k].interactionList.size(); i++) {
					if (maxRank < tree[j][k].L[i].cols())
						maxRank = tree[j][k].L[i].cols();
				}
			}
		}
		return maxRank;
	}

	void evaluate_NearField() { // evaluating at chargeLocations
		// #pragma omp parallel for reduction(+:assTime,matVecTime)
		double assTimeLevel = 0.0;
		double matVecTimeLevel = 0.0;
		#pragma omp parallel
		{
			double start, end;
			double assTimeThread = 0.0;
			double matVecTimeThread = 0.0;
			#pragma omp for nowait
			for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
				for (size_t n = 0; n < 26; n++) {
					int nn = tree[nLevels][k].neighborNumbers[n];
					if(nn != -1) {
						start = omp_get_wtime();
						Eigen::MatrixXd R = K->getMatrix(tree[nLevels][k].chargeLocations, tree[nLevels][nn].chargeLocations);
						end = omp_get_wtime();
						assTimeThread += end-start;

						start = omp_get_wtime();
						tree[nLevels][k].potential += R*tree[nLevels][nn].charges;
						end = omp_get_wtime();
						matVecTimeThread += end-start;
					}
				}
				start = omp_get_wtime();
				Eigen::MatrixXd R = K->getMatrix(tree[nLevels][k].chargeLocations, tree[nLevels][k].chargeLocations);
				end = omp_get_wtime();
				assTimeThread += end-start;

				start = omp_get_wtime();
				tree[nLevels][k].potential += R*tree[nLevels][k].charges; //self Interaction
				end = omp_get_wtime();
				matVecTimeThread += end-start;
			}
			#pragma omp critical
			{
				if (matVecTimeLevel < matVecTimeThread) {
					matVecTimeLevel = matVecTimeThread;
				}
				if (assTimeLevel < assTimeThread) {
					assTimeLevel = assTimeThread;
				}
			}
		}
		assTime += assTimeLevel;
		matVecTime += matVecTimeLevel;
	}

	void collectPotential(Eigen::VectorXd &potential) {
		potential = Eigen::VectorXd::Zero(N);
		// #pragma omp parallel for
		for (size_t j = 2; j <= nLevels; j++) {
			int start = 0;
			Eigen::VectorXd potentialTemp = Eigen::VectorXd::Zero(N);
			// #pragma omp parallel for
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) { //using the fact that all the leaves have same number of particles
				// potentialTemp.segment(pow(8,nLevels-j)*nParticlesInLeaf*k, tree[j][k].potential.size()) = tree[j][k].potential;
				potentialTemp.segment(start, tree[j][k].potential.size()) = tree[j][k].potential;
				start += tree[j][k].potential.size();
			}
			// if(j==nLevels)
			// reorder(potentialTemp);
			potential = potential + potentialTemp;
		}
	}

	void reorder(Eigen::VectorXd &potential) {
		Eigen::VectorXd potentialTemp = potential;
		int start = 0;
		// std::cout << "index: " << std::endl;
		for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
			for (size_t i = 0; i < tree[nLevels][k].chargeLocations.size(); i++) {
				int index = tree[nLevels][k].chargeLocations[i];
				// std::cout << index << std::endl;
				potential(index) = potentialTemp(start);
				start++;
			}
			// std::cout << "---------" << std::endl;
		}
	}

	void findMemory(double &sum) {
		sum = 0;
		for (size_t j = 2; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				for (size_t i = 0; i < tree[j][k].interactionList.size(); i++) {
					int ki = tree[j][k].interactionList[i];
					sum += tree[j][k].L[i].cols()*tree[j][k].chargeLocations.size();
					sum += tree[j][k].L[i].cols()*tree[j][k].L[i].cols();
					sum += tree[j][ki].chargeLocations.size()*tree[j][k].L[i].cols();
				}
			}
		}
		for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
			sum += tree[nLevels][k].chargeLocations.size()*tree[nLevels][k].chargeLocations.size(); //self
			for (size_t n = 0; n < 26; n++) {
				int nn = tree[nLevels][k].neighborNumbers[n];
				if(nn != -1) {
					sum += tree[nLevels][k].chargeLocations.size()*tree[nLevels][nn].chargeLocations.size();
				}
			}
		}
	}

	void findMemory2(double &sum) {
		sum = 0;
		for (size_t j = 2; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				sum += tree[j][k].L2P.size();
			}
		}
		for (size_t j = 2; j <= nLevels; j++) {
			for (size_t k = 0; k < nBoxesPerLevel[j]; k++) {
				for(int in=0; in<tree[j][k].interactionList.size(); ++in) {
					int kIL = tree[j][k].interactionList[in];
						sum += tree[j][k].M2L[kIL].size();
				}
			}
		}

		for (size_t k = 0; k < nBoxesPerLevel[nLevels]; k++) {
			sum += tree[nLevels][k].chargeLocations.size()*tree[nLevels][k].chargeLocations.size(); //self
			for (size_t n = 0; n < 26; n++) {
				int nn = tree[nLevels][k].neighborNumbers[n];
				if(nn != -1) {
					sum += tree[nLevels][k].chargeLocations.size()*tree[nLevels][nn].chargeLocations.size();
				}
			}
		}
	}

};

#endif
