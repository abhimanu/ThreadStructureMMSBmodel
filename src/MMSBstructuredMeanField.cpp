//============================================================================
// Name        : ThreadStructuredMMSBforForums.cpp
// Author      : Abhimanu Kumar
// Version     :
// Copyright   : Your copyright notice
// Description : MMSB in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdlib.h>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include "csv_parser.hpp"
#include <boost/multi_array.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/random.hpp>
#include <boost/math/special_functions/digamma.hpp>
#include <boost/math/special_functions/gamma.hpp>

#include "Utils.h"
#include <math.h>

using namespace std;
using namespace boost::numeric::ublas;


template <class T>
void printMat(matrix<T> *mat, int M, int N) {
	for (int k = 0; k < M; ++k) {
		for (int j = 0; j < N; ++j) {
			cout << (*mat)(k,j) << "," ;
		}
		cout << endl;
	}
}


void printMat3D(boost::multi_array<float,3> *mat, int M, int N, int P) {
	for (int k = 0; k < M; ++k) {
		for (int j = 0; j < N; ++j) {
			for (int i = 0; i < P; ++i) {
				cout << (*mat)[k][j][i] << " " ;
			}
			cout << "||==||" ;
		}
		cout << endl;
	}
}

class MMSB{
private:
	boost::numeric::ublas::vector<float>* alpha;
	matrix<float>* gamma;
	boost::multi_array<float,4>* phiPQ;
	matrix<float>* B;
	int num_users;
	int K;
	Utils* utils;
	matrix<float>* bDenomSum;
	matrix<int>* inputMat;
	float multiplier;
	static const  int variationalStepCount=10;
	static const float threshold=1e-5;
	static const float alphaStepSize=1e-6;
	static const float stepSizeMultiplier=0.5;
	static const float globalThreshold=1e-4;

	void initialize(int num_users, int K, matrix<int>* inputMat){
		gamma = new matrix<float>(num_users,K);
		B = new matrix<float>(K,K);
		alpha = new boost::numeric::ublas::vector<float>(K);
		phiPQ = new boost::multi_array<float, 4>(boost::extents[K][K][num_users][num_users]);

		multiplier = alphaStepSize;
		this->inputMat = inputMat;
		this->num_users = num_users;
		this->K=K;
		initializeAlpha();
		initializeB();
		initializeGamma();
		for(int k=0;k<K;k++)cout<<(*alpha)(k)<<" ";
		cout<<endl;
		printMat(B,K,K);
		printMat(inputMat,num_users,num_users);
		printMat(gamma,num_users,K);
	};

//	void normalizePhi(){
//		cout<<"phi\n";
//		for (int i = 0; i < num_users; ++i) {
//			for (int j = 0; j < num_users; ++j) {
//				normalizePhiK(i,j);
//			}
//			cout<<endl;
//		}
//	};



public:
	MMSB(Utils *);
	void getParameters(matrix<int>* matFile, int num_users, int K, int iter_threshold, int inner_iter);
	float getUniformRandom();
//	matrix<float>* updatePhiVariational(int p, int q, float sumGamma_p, float sumGamma_q);
	float getVariationalLogLikelihood();
//	void updateB(int p, int q, matrix<float>* oldPhi_pq);
	void updateB();
	void updateGamma(int p);
	float dataFunction(int p, int q, int g, int h);
	float updateGammasAndBlockMat(int inner_iter);
	void variationalUpdatesPhi(int p, int q);
	float getMatrixRowSum(matrix<float>* mat, int row_id, int num_cols);
	void printPhi(int p, int q);
	void printPhiFull();
	float getDigamaValue(float value);
//	void normalizePhiK(int p, int q, bool debugPrint=false);
//	void copyAlpha(boost::numeric::ublas::vector<float>* oldAlpha);
//	void updateAlpha(bool flagLL);
	void initializeGamma();
	void initializeAlpha();
	void initializeB();
	matrix<float>* getPis();
	boost::numeric::ublas::vector<float>* getVecH();
	boost::numeric::ublas::vector<float>* getVecG();
};

MMSB::MMSB(Utils* utils){
	this->utils = utils;
}

void MMSB::initializeAlpha(){
	for (int k = 0; k < K; ++k) {
		(*alpha)(k)= 0.5+(getUniformRandom()-0.5)*0.1;
	}
}

//void MMSB::normalizePhiK(int p, int q, bool debugPrint){ // TODO: change phi coz it is K*K*N*N
//	float topic_sum1 = 0;
//	float topic_sum2 = 0;
//	for (int k = 0; k < K; ++k) {
//		topic_sum1 += (*phiPToQ)[p][q][k];
//		topic_sum2 += (*phiPFromQ)[p][q][k];
//	}
//	if(debugPrint){
//		cout<<"topic_sum1 "<<topic_sum1<<endl;
//		cout<<"topic_sum2 "<<topic_sum2<<endl;
//	}
//	for (int k = 0; k < K; ++k) {
//		(*phiPToQ)[p][q][k] = ((*phiPToQ)[p][q][k])/topic_sum1;
//		(*phiPFromQ)[p][q][k] = ((*phiPFromQ)[p][q][k])/topic_sum2;
//		if(debugPrint){
//			cout<<(*phiPToQ)[p][q][k]<<" ";
//			cout<<(*phiPFromQ)[p][q][k]<<" ";
//		}
//	}
//	if(debugPrint)
//		cout<<endl;
//}

float MMSB::dataFunction(int p,int q,int g,int h){
	return (*inputMat)(p,q)*log((*B)(g,h)) + (1-(*inputMat)(p,q))*log((1-(*B)(g,h)));
}

float MMSB::getVariationalLogLikelihood(){                // TODO: change phi coz it is K*K*N*N 
float ll=0;
	for (int p = 0; p < num_users; ++p) {
		float alphaSum = 0;
		float gammaSum = 0;
		for (int k = 0; k < K; ++k){
			alphaSum+=alpha->operator ()(k);
			gammaSum += gamma->operator ()(p,k);
		}
		ll+=lgamma(alphaSum);																	//line 4
		ll-=lgamma(gammaSum);																	//line 5
		for (int k = 0; k < K; ++k) {
			ll-=lgamma(alpha->operator ()(k));													//line 4
			float digammaTerm = (getDigamaValue(gamma->operator ()(p,k))-getDigamaValue(gammaSum));
			ll+= ((alpha->operator ()(k)-1)*(digammaTerm));										//line 4

			ll+=lgamma(gamma->operator ()(p,k));												//line 5
			ll-=((gamma->operator ()(p,k)-1)*(digammaTerm));									//line 5
		}

		for (int q = 0; q < num_users; ++q) {
			if(p==q)
				continue;
			for (int g = 0; g < K; ++g) {
				float gammaSum_p =0;
				float gammaSum_q =0;
				for (int h = 0; h < K; ++h) {
					ll+=((*phiPQ)[g][h][p][q])*dataFunction(p,q,g,h);	// line 1 of MMSB
					gammaSum_p += gamma->operator ()(p,h);
					gammaSum_q += gamma->operator ()(q,h);
				}
				for (int h = 0; h < K; ++h) {
					float digammaTerm_p = (getDigamaValue(gamma->operator ()(p,g))-getDigamaValue(gammaSum_p));
					float digammaTerm_q = (getDigamaValue(gamma->operator ()(q,h))-getDigamaValue(gammaSum_q));
					ll+= ((*phiPQ)[g][h][p][q])*digammaTerm_p;                                  // line 2
					ll+= ((*phiPQ)[g][h][p][q])*digammaTerm_q;                                  // line 3
					ll-= ((*phiPQ)[g][h][p][q])*log(((*phiPQ)[g][h][p][q]));                    // line 8
				}
			}
		}
	}
	return ll;
}

float MMSB::getDigamaValue(float value){
	return boost::math::digamma(value);
}

void MMSB::getParameters(matrix<int>* inputMat, int num_users, int K, int iter_threshold, int inner_iter){ // TODO: change phi coz it is K*K*N*N 
	initialize(num_users, K, inputMat);
	cout<<"ll-0"<<getVariationalLogLikelihood()<<endl;
	boost::numeric::ublas::vector<float>* oldAlpha = new boost::numeric::ublas::vector<float>(K);
//	copyAlpha(oldAlpha);
	float newLL = 0;//getVariationalLogLikelihood();
	float oldLL = 0;
	int iter=0;
//	int iter_threshold = 30;
//	int inner_iter = 4;
	do{
        iter++;
		cout<<"iter "<<iter<<endl;
		oldLL=newLL;
		newLL=updateGammasAndBlockMat(inner_iter);
		if(iter>=iter_threshold)
			break;
	}while(1);//abs(oldLL-newLL)>globalThreshold);
	matrix<float>* pi = getPis();
	cout<<"PI\n";
	printMat(pi,num_users,K);
	cout<<"B\n";
	printMat(B,K,K);
	cout<<"Gamma\n";
	printMat(gamma,num_users,K);
}


float MMSB::updateGammasAndBlockMat(int inner_iter){//(gamma,B,alpha,Y,inner_iter){
	float ll=0;
	for(int i=1; i<=inner_iter;i++){
//		cout<<"i "<<i<<endl;
		for(int p=0; p<num_users; p++){
			for(int q=0; q<num_users; q++){
				if(p==q)
					continue;
				variationalUpdatesPhi(p,q);//Y(p,q),phi(:,:,p,q),B,gamma(p,:),gamma(q,:),alpha,inner_iter);
//				cout<<"p,q "<<p<<" "<<q<<"||";
//				printPhi(p,q);
			}
		}                          
//        cout<<"update Gamma and B now"<<endl;
//		printPhiFull();
		for(int p=0; p<num_users; p++){
			updateGamma(p);
//			cout<<"update B now"<<endl;
//			updateB();
//			cout<<"updated both B and Gamma"<<endl;
		}
		cout<<"Gamma\n";
		printMat(gamma, num_users, K);
		cout<<"B\n";
		updateB();
		printMat(B,K,K);
		ll = getVariationalLogLikelihood();
		cout<<ll<<endl;
		//       pi = gamma./repmat(sum(gamma,2),1,K)
	}
	return ll;

}

void MMSB::printPhi(int p, int q){
	for(int g=0;g<K; g++){
		for(int h=0;h<K;h++)
			cout<<(*phiPQ)[g][h][p][q]<<" ";
		cout<<";";
	}
	cout<<"||||||||||||||";

}
void MMSB::printPhiFull(){
	cout<<endl<<endl;
	for(int p=0; p<num_users; p++){
		for(int q=0; q<num_users; q++){
			cout<<"p,q "<<p<<" "<<q<<"||";
			for(int g=0;g<K; g++){
				for(int h=0;h<K;h++)
					cout<<(*phiPQ)[g][h][p][q]<<" ";
				cout<<";";
			}
			cout<<"||||||||||||||";
		}
	}
	cout<<endl;
}

void MMSB::variationalUpdatesPhi(int p, int q){
	//N = size(Y,1);
	//K = size(alpha,2);
	boost::numeric::ublas::vector<float>* deriv_phi_p = new boost::numeric::ublas::vector<float>(K);
	boost::numeric::ublas::vector<float>* deriv_phi_q = new boost::numeric::ublas::vector<float>(K);
	int Y_pq = (*inputMat)(p,q);
	float digamma_p_sum = getDigamaValue(getMatrixRowSum(gamma,p,K));
	float digamma_q_sum = getDigamaValue(getMatrixRowSum(gamma,q,K));
//	cout<<digamma_q_sum<<endl;
//	cout<<digamma_p_sum<<endl;

	float phi_sum = 0;
	for(int g=0;g<K;g++){
		for(int h=0;h<K;h++){
			(*phiPQ)[g][h][p][q] = exp(Y_pq*log((*B)(g,h)) + (1-Y_pq)*log(1-(*B)(g,h)) 
				+ (getDigamaValue((*gamma)(p,g)) - digamma_p_sum)
				+ (getDigamaValue((*gamma)(q,h)) - digamma_q_sum));
			phi_sum+=(*phiPQ)[g][h][p][q];
		}
	}

//	cout<<"variationalUpdatesPhi"<<endl;

	for(int g=0;g<K;g++){
		for(int h=0;h<K;h++){
			(*phiPQ)[g][h][p][q] = ((*phiPQ)[g][h][p][q])/phi_sum ;
//			cout<<(*phiPQ)[g][h][p][q]<<" ";
		}
	}
//	cout<<endl;
}

float MMSB::getMatrixRowSum(matrix<float>* mat, int row_id, int num_cols){
	float row_sum = 0;
	for(int j=0; j<num_cols; j++)
		row_sum+=(*mat)(row_id,j);
	return row_sum;
}

matrix<float>* MMSB::getPis(){
	matrix<float>* pi = new matrix<float>(num_users,K);
	for (int p = 0; p < num_users; ++p) {
		float sumPk = 0;		for (int k = 0; k < K; ++k) {
			sumPk+=((*gamma)(p,k));
		}
		for (int k = 0; k < K; ++k) {
			((*pi)(p,k)) = ((*gamma)(p,k))/sumPk;
		}
	}
	return pi;
}

//void MMSB::copyAlpha(boost::numeric::ublas::vector<float>* copyAlpha){
//	for (int k = 0; k < K; ++k) {
//		copyAlpha->operator ()(k) = alpha->operator ()(k);
//	}
//}
//
//void MMSB::updateAlpha(bool flagLL){
//	// this part is coded by approximating H as a diagnoal + rank one matrix (Blei's LDA paper)
//	boost::numeric::ublas::vector<float>* hVec = getVecH();
//	float sumAlpha = 0;
//	for (int k = 0; k < K; ++k) {
//		sumAlpha+=alpha->operator ()(k);
//	}
//	float z = -num_users*utils->trigamma(sumAlpha);
//	float gByhSum = 0, sumHinv=0;
//	boost::numeric::ublas::vector<float>* gVec = getVecG();
//	for (int k = 0; k < K; ++k) {
//		gByhSum += ((gVec->operator ()(k))/(hVec->operator ()(k)));
//		sumHinv +=(1.0/(hVec->operator ()(k)));
//	}
//	float c = gByhSum/((1.0/z)+sumHinv);
//
//	if(!flagLL)
//		multiplier=multiplier*stepSizeMultiplier;
//	for (int k = 0; k < K; ++k) {
//		alpha->operator ()(k) = alpha->operator ()(k) - ((gVec->operator ()(k) - c)/hVec->operator ()(k))*multiplier;
//	}
//}

boost::numeric::ublas::vector<float>* MMSB::getVecG(){					// gradient of L w.r.t alpha
	boost::numeric::ublas::vector<float>* vecG = new boost::numeric::ublas::vector<float>(K);
	boost::numeric::ublas::vector<float> vecSumGammaP(num_users);
	float sumAlpha = 0;
	for (int k = 0; k < K; ++k) {
		sumAlpha+=alpha->operator ()(k);
	}
	for (int p = 0; p < num_users; ++p) {
		vecSumGammaP(p)=0;
		for (int k = 0; k < K; ++k) {
			vecSumGammaP(p)+=gamma->operator ()(p,k);
		}
	}
	float sumPgradient=0;
	for (int k = 0; k < K; ++k) {
		vecG->operator ()(k) = num_users*(getDigamaValue(sumAlpha)-getDigamaValue(alpha->operator ()(k)));
		sumPgradient = 0;
		for (int p = 0; p < num_users; ++p) {
			sumPgradient += (getDigamaValue(gamma->operator ()(p,k)) - getDigamaValue(vecSumGammaP(p)));
		}
		vecG->operator ()(k) = vecG->operator ()(k) + sumPgradient;
	}
	return vecG;
}

boost::numeric::ublas::vector<float>* MMSB::getVecH(){					// hessian of L w.r.t alpha
	boost::numeric::ublas::vector<float>* vecH = new boost::numeric::ublas::vector<float>(K);
	for (int k = 0; k < K; ++k) {
		vecH->operator ()(k)= num_users*(utils->trigamma(alpha->operator ()(k)));
	}
	return vecH;
}


void MMSB::initializeB(){
	bDenomSum = new matrix<float>(K,K);
	for (int g = 0; g < K; ++g) {
		for (int h = 0; h < K; ++h) {
			(*B)(g,h)=1e-6;
		}
		(*B)(g,g)=0.99;
	}
}


void MMSB::updateB(){
	for(int g=0; g<K; g++){
		for(int h=0;h<K;h++){
			float B_gh = 0;
			float den_gh = 0;
			for(int p=0; p<num_users;p++){
				for(int q=0; q<num_users;q++){
					if(p==q)
						continue;
					B_gh+= ((*inputMat)(p,q)*((*phiPQ)[g][h][p][q]));
					den_gh += ((*phiPQ)[g][h][p][q]);
				}
			}
			(*B)(g,h) = (B_gh + 1e-10)/(den_gh + 2e-10);
			if ((*B)(g,h) >= 1)								// throws up nan error otherwise
				(*B)(g,h) = 1-1e-5;
		}
	}
}




void MMSB::initializeGamma(){

	for (int p = 0; p < num_users; ++p) {
		for (int k = 0; k < K; ++k) {
			(*gamma)(p,k)=(*alpha)(k)+(getUniformRandom()-0.5)*0.1;
//			cout<<(*alpha)(k)<<" "<<(*gamma)(p,k)<<" ";

		}
//		cout<<endl;
	}
}


void MMSB::updateGamma(int p){
	float gamma_pk=0;
	for(int k=0; k<K; k++){
		gamma_pk = (*alpha)(k);
		for(int q=0; q<num_users; q++){
//			cout<<"k,q "<<k<<" "<<q<<"|";
			if(p==q)
				continue;
			for(int h=0; h<K; h++){
				gamma_pk+= ((*phiPQ)[k][h][p][q] + (*phiPQ)[h][k][q][p]);
			}
		}
		(*gamma)(p,k)=gamma_pk;
//		cout<<"gamma_pk "<<p<<","<<k<<","<<gamma_pk<<";";
	}
//	cout<<endl;
}




float MMSB::getUniformRandom(){
	boost::mt19937 rng;
	rng.seed(static_cast<unsigned int>(std::time(0)));
//	rng.distribution().reset();
	static boost::uniform_01<boost::mt19937> zeroone(rng);
	return zeroone();

}




int main(int argc, char** argv) {
//	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!



	Utils *utilsClass = new Utils();

	matrix<int>* matFile =  utilsClass->readCsvToMat((char *) ("monkLK.csv"), 18, 18);
//	cout<<matFile(0,0)<<matFile(0,2)<<endl;

	MMSB* mmsb = new MMSB(utilsClass);
	mmsb->getParameters(matFile, atoi(argv[3]), atoi(argv[4]), atoi(argv[1]), atoi(argv[2]));
//	cout<<mmsb->getDigamaValue(20)<<endl;
//	cout<<mmsb->getDigamaValue(30)<<endl;
//	cout<<mmsb->getDigamaValue(10)<<endl;
//	cout<<mmsb->getDigamaValue(1)<<endl;
//	cout<<mmsb->getDigamaValue(0)<<endl;
//	cout<<mmsb->getDigamaValue(101)<<endl;
	return 0;



}
