#include "Requests.hpp"

//====================================================================================
// ** AbstractHeaderCoder
//====================================================================================
Requests::Requests(IBank* inputBank, string outputFilename, Graph graph, Kmer<>::ModelCanonical model, Partition<kmer_count> & solidCollection, size_t kmerSize){
	cout << "entering requests creator" << endl;

	req_buffer_size = 1024;
	end_requests = false;

	_inputBank = inputBank;
	_itBank = _inputBank->iterator();
	_itBanks =  _itBank->getComposition();
	_nbBanks = _itBanks.size();

	cout << "nb banks : " << _nbBanks << endl; 

	_graph = graph;
	_model = model;

	_kmerSize = kmerSize;
	_kmerModel = new KmerModel(_kmerSize);

	_outputFilename = outputFilename; 
	_solidFileSize = solidCollection.getNbItems();
	_nb_kmers_infile = solidCollection.getNbItems();
	_itKmers = solidCollection.iterator();

	cout << "nb kmers : " << _nb_kmers_infile << endl;

	_signature_array =  (unsigned char  *)  malloc(_solidFileSize*sizeof(char));
    _color_array =  (unsigned char  *)  malloc(_solidFileSize*sizeof(char));


    memset(_signature_array, 0, _solidFileSize);
    memset(_color_array, 0, _solidFileSize);



    const char* signatures_file_path = outputFilename.c_str();
	char* signatures_file_ext = ".signatures_file";
	char signatures_file_path_ext[1024];
	strcpy(signatures_file_path_ext, signatures_file_path);
	strcat(signatures_file_path_ext, signatures_file_ext);
	printf("signatures_file_path_ext : %s", signatures_file_path_ext);

	const char* colors_file_path = outputFilename.c_str();
	char* colors_file_ext = ".colors_file";
	char colors_file_path_ext[1024];
	strcpy(colors_file_path_ext, colors_file_path);
	strcat(colors_file_path_ext, colors_file_ext);

    FILE* signatures_file = fopen(signatures_file_path_ext, "r");
    FILE* colors_file = fopen(colors_file_path_ext, "r");

    fread(_signature_array, 1, _solidFileSize, signatures_file); 

    fread(_color_array, 1, _solidFileSize, colors_file); 

    fclose(signatures_file);
    fclose(colors_file);

}

void Requests::printSignatures(){
	cout << "test signatures" << endl;

	cout << "signatures : \n" << endl;

	for (int i=0; i < _nb_kmers_infile; ++i){
		cout << std::bitset<8>(_signature_array[i]) << endl;
	}
}

void Requests::printColors(){
	cout << "test signatures" << endl;

	cout << "colors : \n" << endl;

	for (int i=0; i < _nb_kmers_infile; ++i){
		cout << std::bitset<8>(_color_array[i]) << endl;
	}
}




void Requests::printSequences(){

    for (_itBank->first(); !_itBank->isDone(); _itBank->next())
	{
		
		std::cout <<  _itBank->item().toString()  << endl << endl;

}
}

void Requests::printKmers(){
for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next()){
			
	std::cout <<  _model.toString (_itKmers->item().getValue())  << endl;
	}

}

void Requests::printMPHFIndexes(){
for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next()){
			
	Node node(Node::Value(_itKmers->item().getValue()));
		printf("_graph.nodeMPHFIndex(node) : %d\n", _graph.nodeMPHFIndex(node));
	}

}

void Requests::printTestAll(){

 for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next())
	{
		
		
	Node node(Node::Value(_itKmers->item().getValue()));
	printf("_graph.nodeMPHFIndex(node) %d\n", _graph.nodeMPHFIndex(node));
		
	std::cout <<  _model.toString (_itKmers->item().getValue())  << "\t" <<
	std::bitset<8>(_color_array[_graph.nodeMPHFIndex(node)]) << "\t" <<
	std::bitset<8>(_signature_array[_graph.nodeMPHFIndex(node)]) << std::endl;

	} 
}

void Requests::printNbBanks(){
	std::cout << "number of data sets : " << _nbBanks << std::endl;
}

int Requests::getNbBanks(){
	return _nbBanks;
}

void Requests::printKmerSize(){
	std::cout << "kmer size : " << _kmerSize << std::endl << std::endl;
}

int Requests::getKmerSize(){
	return _kmerSize;
}

bool Requests::isKmerInData(char* kmer_chars){


	kmer_type kmer = _kmerModel->codeSeed(kmer_chars, Data::ASCII).value() ;
	Node node = Node(Node::Value(kmer));

	return _graph.contains(node);
}

int Requests::getNbDataContainingKmer(char* kmer_chars)
{

	if (!this->isKmerInData(kmer_chars)){
		//cout << "0" << endl;
		return 0;
	}

	else{

		kmer_type kmer = _kmerModel->codeSeed(kmer_chars, Data::ASCII).value() ;
		Node node = Node(Node::Value(kmer));

		bitset<8> kmer_colors = _color_array[_graph.nodeMPHFIndex(node)];
		return kmer_colors.count();
	}
	
}

bitset<8> Requests::getDataContainingKmer(char* kmer_chars){


	if (!this->isKmerInData(kmer_chars)){
		bitset<8> data;
		return data;
	}

	else{

		kmer_type kmer = _kmerModel->codeSeed(kmer_chars, Data::ASCII).value() ;
		Node node = Node(Node::Value(kmer));

		return _color_array[_graph.nodeMPHFIndex(node)];
	}

}

void Requests::fgetRequests(){


	cout << endl << endl <<
		"############# debug #############" << endl << endl <<
		"sig \t\tto print sinatures" << endl <<
		"col \t\tto print colors" << endl <<
		"seq \t\tto print sequences" << endl <<
		"kmers \t\tto print kmers" << endl <<
		"mphf \t\tto print mphf indexes" << endl <<
		"testall \tto print kmers, indexes in mphf, color and signature" << endl << endl << endl <<
		
		"############ requests ############" << endl << 
		
		"nb ds \t\tto get the number of datasets in the file" << endl << endl <<
		
		"kmer s \t\tto get size of kmers" << endl <<
		"kmer p \t\tto know if the kmer is present in the data" << endl <<
		"kmer h \t\tto know in how many datasets the kmer is present" << endl <<
		"kmer d \t\tto know in which datasets the kmer is present" << endl << endl <<

		"seq p \t\tto know if the sequence is present in the data" << endl <<
		"seq h \t\tto know in how many datasets the sequence is present" << endl <<
		"seq d \t\tto know in which datasets the sequence is present" << endl << endl <<
		
		"q \t\tto quit" << endl << endl;

		fgets(request, req_buffer_size, stdin);
		request[strlen(request)-1]='\0';

		//cout << req << " strlen req : " << strlen(req) << endl;
		cout << endl;

		//debug
		if (strcmp(request, "sig")==0){
			this->printSignatures();
		}

		if (strcmp(request, "col")==0){
			this->printColors();
		}

		if (strcmp(request, "seq")==0){
			this->printSequences();
		}

		if (strcmp(request, "kmers")==0){
			this->printKmers();
		}

		if (strcmp(request, "mphf")==0){
			this->printMPHFIndexes();
		}

		if (strcmp(request, "testall")==0){
			this->printTestAll();
		}

		//requests

		char kmer_req[1024];
		char sequence_req[1024];

		if (strcmp(request, "nb ds")==0){
			this->printNbBanks();
		}

		if (strcmp(request, "kmer s")==0){
			this->printKmerSize();
		}

		if (strcmp(request, "kmer p")==0){

			if(this->fgetKmer(kmer_req)){

				if (this->isKmerInData(kmer_req)){
					std::cout << kmer_req << " is present" << std::endl;
				}
				else{
					std::cout << kmer_req << " is not present" << std::endl;
				}
			}
			kmer_req[0] = '\0';
		}

		if (strcmp(request, "kmer h")==0){

			if (this->fgetKmer(kmer_req)){
				int nbData = this->getNbDataContainingKmer(kmer_req);
				cout << nbData << endl;
			}
		}

		if (strcmp(request, "kmer d")==0){

			if (this->fgetKmer(kmer_req)){
				bitset<8> data = this->getDataContainingKmer(kmer_req);

				if (data.none()){
					cout <<  kmer_req << " is not present in any dataset" << endl;
				}

				else{
					int nbBanks = this->getNbBanks();

					cout <<  kmer_req << " is present in the following dataset : " << endl;
					for (int i=0; i<nbBanks; ++i){
						if (data.test(i)){
							cout << i << endl;
						}
					}
				}
			}
		}

		if (strcmp(request, "q")==0){
			this->end_requests = true;
		}

}

bool Requests::fgetKmer(char* kmer){
	std::cout << "enter the kmer :" << std::endl << std::endl;
	fgets(kmer, 1024, stdin);
	kmer[strlen(kmer)-1]='\0';

	if (strlen(kmer) != _kmerSize){
		cout << "error : you gave a kmer of size : " << strlen(kmer) << 
		" and kmerSize is : " << _kmerSize << endl;
		return false;
	}

	return true;
}