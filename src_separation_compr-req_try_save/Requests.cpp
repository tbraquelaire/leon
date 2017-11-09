#include "Requests.hpp"

/*****constructor*****/
Requests::Requests(/*IBank* inputBank,*/ string inputFilename, Graph graph, 
	Kmer<>::ModelCanonical model, 
	Partition<kmer_count> & solidCollection, size_t kmerSize, 
	Hash16<kmer_type, u_int32_t >  * anchorKmers,
	Hash16<kmer_type, u_int32_t >  * anchorKmersSorted,
	Leon* leon,
	DnaDecoder* dnadec): _generalModel(256), _anchorDictModel(5){
	
	cerr << "Requests::Requests - entering requests creator" << endl;

	req_buffer_size = 1024;
	end_requests = false;
	sequenceMaxSize = 1024;


	//not used anymore since requests separated from compression
	/*
	_inputBank = inputBank;
	_itBank = _inputBank->iterator();
	_itBanks =  _itBank->getComposition();
	_nbBanks = _itBanks.size();

	cout << "nb banks : " << _nbBanks << endl; 
	*/

	_leon = leon;
	_graph = graph;
	_model = model;

	_kmerSize = kmerSize;
	_kmerModel = new KmerModel(_kmerSize);
	_anchorKmers = anchorKmers;
	_anchorKmersSorted = anchorKmersSorted;
	_anchorAdress = 0;

	_inputFilename = inputFilename; 
	_solidFileSize = solidCollection.getNbItems();
	_nb_kmers_infile = solidCollection.getNbItems();
	_itKmers = solidCollection.iterator();

	_dnadec = dnadec;
	for(int i=0; i<CompressionUtils::NB_MODELS_PER_NUMERIC; i++){
		_numericModel.push_back(Order0Model(256));
		_nbReadsPerAnchorModel.push_back(Order0Model(256));
	}

	cout << "nb kmers : " << _nb_kmers_infile << endl;

	/** We look for the begining of the suffix. */
	int lastindex = inputFilename.find_last_of (".");
	
	/** We build the strings for file names. */
	string pathToFileName = inputFilename.substr(0,lastindex);
	string extension = inputFilename.substr(lastindex+1);
	cerr << "Requests::Requests - extension : " << extension << endl;

	/** Getting the compression mode file. */
	if (extension.compare("peacock") == 0)
	{
		_orderReads = true;
	}
	else
	{
		_orderReads = false;
	}

	/** Getting the signatures and colors array. */
	_signature_array =  (unsigned char  *)  malloc(_solidFileSize*sizeof(char));
    _color_array =  (unsigned char  *)  malloc(_solidFileSize*sizeof(char));

    memset(_signature_array, 0, _solidFileSize);
    memset(_color_array, 0, _solidFileSize);

    string signaturesFile = pathToFileName + ".signatures_file";
	string colorsFile = pathToFileName + ".colors_file";

	FILE* signatures_file = fopen(signaturesFile.c_str(), "r");
    FILE* colors_file = fopen(colorsFile.c_str(), "r");

    fread(_signature_array, 1, _solidFileSize, signatures_file); 

    fread(_color_array, 1, _solidFileSize, colors_file); 

    // more simple way to do this (above), to delete 
	/*
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
    */


    fclose(signatures_file);
    fclose(colors_file);

}

/*****utilities*****/

bool Requests::getSequenceKmer(const char* seq, uint pos, char* kmer){
	//cout << "\tdebug Requests::getSequenceKmer - before if" << endl;
	if ((pos < 0) || (pos > strlen(seq)-_kmerSize)){
		return false;
	}
	//cout << "\tdebug Requests::getSequenceKmer - after if" << endl;
	//cout << "\tdebug Requests::getSequenceKmer - pos : " << pos << endl;

	strncpy(kmer, seq+pos, _kmerSize);
	//cout << "\tdebug Requests::getSequenceKmer - after strncpy" << endl;
	kmer[_kmerSize] = '\0';
	//cout << "\tdebug Requests::getSequenceKmer - after \0 affectation" << endl;
	return true;
}

bool Requests::getNextAnchor(char* sequence, uint* pos, char* anchor, u_int32_t anchorAddress){

	char kmer[_kmerSize+1];

	while (this->getSequenceKmer(sequence, *pos, kmer)){

		if (this->anchorExist(kmer, &anchorAddress)){
			strncpy(anchor, kmer, _kmerSize+1);
			return true;
		}
		(*pos)++;
	}

	return false;
}

void Requests::fillSequenceAnchorsDict(Hash16<kmer_type, list<u_int32_t>* >  * sequenceAnchorKmers,
										char* sequence){

	u_int32_t anchorAddress;
	u_int32_t pos = 0;
	list<u_int32_t>* listPos;
	char kmer_chars[_kmerSize+1];
	char anchor_chars[_kmerSize+1];

	while(getSequenceKmer(sequence, pos, kmer_chars))
	{	

		if (getNextAnchor(sequence, &pos, anchor_chars, anchorAddress)){

			kmer_type anchor = getKmerType(anchor_chars);
	
			/* if list empty, create the list
			** the list is empty if the hash table doesn't contain the kmer 
			** (first time we add it)
			*/
			if (!sequenceAnchorKmers->get(anchor, &listPos)){

				listPos = new list<u_int32_t>();	
			}

			listPos->push_back(pos);
			sequenceAnchorKmers->insert(anchor, listPos);
		}

		pos++;
	}
}

//TODO
//iterate on the hash and not on the sequence
//didn't do it because didn't found how to use cell object
void Requests::emptySequenceAnchorDict(Hash16<kmer_type, list<u_int32_t>* >  * sequenceAnchorKmers, char* sequence){

	u_int32_t anchorAddress;
	list<u_int32_t>* listPos;

	uint nbKmer = 0;
	char kmer[_kmerSize+1];
	char anchor[_kmerSize+1];

	while(getSequenceKmer(sequence, nbKmer, kmer))
	{	
		if (getNextAnchor(sequence, &nbKmer, anchor, anchorAddress)){		

			if (sequenceAnchorKmers->get(getKmerType(anchor), &listPos)){
				delete listPos;
				sequenceAnchorKmers->remove(getKmerType(anchor), &listPos);

			}
		}

		nbKmer++;
	}
}

kmer_type Requests::getKmerType(char* kmer_chars){

	kmer_type kmer = _kmerModel->codeSeed(kmer_chars, Data::ASCII).value() ;

	return kmer;
}

void Requests::getKmerChars(kmer_type kmer, char* kmer_chars){

	//char kmer_chars[_kmerSize+1];
	
	const char* kmerStr = kmer.toString(_kmerSize).c_str();
	strncpy(kmer_chars, kmerStr, _kmerSize);
	kmer_chars[_kmerSize] = '\0';

	//return kmer_chars;
}

void Requests::getSequenceChars(char* sequence_chars, string sequence){

	strcpy(sequence_chars, sequence.c_str());
	sequence_chars[sequence.size()] = '\0';
	//cout << "\tdebug Requests::getSequenceChars - sequence_chars : " << sequence_chars << endl;
	//return sequence_chars;
}

string Requests::getKmerString(kmer_type kmer){
	return kmer.toString(_kmerSize);
}

Node Requests::getKmerNode(char* kmer_chars){

	kmer_type kmer = this->getKmerType(kmer_chars);
	Node node = Node(Node::Value(kmer));

	return node;
}

Node Requests::getKmerNode(kmer_type kmer){

	Node node = Node(Node::Value(kmer));

	return node;
}

int Requests::getNodeMPHFIndex(kmer_type kmer){

	Node node = getKmerNode(kmer);
	return _graph.nodeMPHFIndex(node);
}

int Requests::getNodeMPHFIndex(char* kmer_chars){

	Node node = getKmerNode(kmer_chars);
	return _graph.nodeMPHFIndex(node);
}

//convert to leon method
//TODO : copier find existing anchor (avec revcomp)

kmer_type Requests::getAnchor(ifstream* anchorDictFile, u_int32_t address){
	
	//cout << "debug requests - getAnchor - _vecAnchorKmers[address] before" << endl;
	return _vecAnchorKmers[address];
}

bool Requests::anchorExist(char* kmer_chars, u_int32_t* anchorAddress){
	
	kmer_type kmer, kmerMin;
	kmer = this->getKmerType(kmer_chars);
	kmerMin = min(kmer, revcomp(kmer, _kmerSize));

	return _leon->anchorExist(kmerMin, anchorAddress);
}

unsigned char Requests::getKmerSignature(kmer_type kmer){
	
	Node node = getKmerNode(kmer);
	//cout << "test kmerSignature array : " << (unsigned long) _signature_array[_graph.nodeMPHFIndex(node)] << endl;
	//cout << "test kmerSignature compute :  " << (unsigned long) (hash1(kmer,0) & 255) << endl;
	return _signature_array[_graph.nodeMPHFIndex(node)];
}

bitset<NB_MAX_COLORS> Requests::getReadColor(ReadInfos* ri){
	//cout << "\tdebug  Requests::getReadColor - BEGIN" << endl; 
	kmer_type anchor = ri->anchor;
	int readSize = ri->readSize;
	int anchorPos = ri->anchorPos;

	char read_chars[ri->sread.size()+1];
	getSequenceChars(read_chars, ri->sread);
	//cerr << "debug Requests::getReadColor - ri->read : " << ri->sread << endl;
	//cerr << "debug Requests::getReadColor - csread : " << read_chars << endl;

	bitset<NB_MAX_COLORS> readColor = getKmerColors(anchor);
	uint pos = anchorPos;
	char kmer[_kmerSize+1]; 

	//cout << "\tdebug  Requests::getReadColor - before search" << endl; 
	//we search in right direction fisrt (more chance to eliminates wrong colors in one pass)
	if (anchorPos < (readSize/2)){

		++pos;
		//we search untill only one color remains (or end of read)
		while((getSequenceKmer(read_chars, pos, kmer)) && (readColor.count() > 1)){
			
			//we only take into acount solid kmers
			if (isKmerInGraph(kmer)){
				//cout << "\tdebug  Requests::getReadColor - before getKmerColors" << endl; 
				readColor &= getKmerColors(kmer);
				//cout << "\tdebug  Requests::getReadColor - after getKmerColors" << endl; 
			}
			++pos;
		}

		//we can stop if we have one color in the bitset
		if (readColor.count() <= 1){
			//cout << "\tdebug  Requests::getReadColor - END" << endl;
			return readColor;
		}

		//else we have to finish the search in the other direction		
		pos = anchorPos-1;
		//cout << "\tdebug  Requests::getReadColor - before left search" << endl;
		while(getSequenceKmer(read_chars, pos, kmer) && (readColor.count() > 1)){
			//cout << "\tdebug  Requests::getReadColor - during left search" << endl;
			if (isKmerInGraph(kmer)){
				//cout << "\tdebug  Requests::getReadColor - before getKmerColors" << endl; 
				readColor &= getKmerColors(kmer);
				//cout << "\tdebug  Requests::getReadColor - before getKmerColors" << endl; 
			}
			--pos;
		}
		
		//cout << "\tdebug  Requests::getReadColor - END" << endl;
		return readColor;
	}

	//we search in the left direction first
	else{

		--pos;
		while(getSequenceKmer(read_chars, pos, kmer) && (readColor.count() > 1)){
			
			if (isKmerInGraph(kmer)){
				readColor &= getKmerColors(kmer);
			}
			--pos;
		}

		if (readColor.count() <= 1){
			//cout << "\tdebug  Requests::getReadColor - END" << endl;
			return readColor;
		}

		pos = anchorPos+1;
		while(getSequenceKmer(read_chars, pos, kmer) && (readColor.count() > 1)){
			
			if (isKmerInGraph(kmer)){
				readColor &= getKmerColors(kmer);
			}
			++pos;
		}
		
		//cout << "\tdebug  Requests::getReadColor - END" << endl;
		return readColor;
	}

}


// decode functions

void Requests::initializeRangeDecoder(){

	/*if (_orderReads){
		_decodeFilename = _inputFilename + ".peacock";
	}
	else{
		_decodeFilename = _inputFilename + ".leon";
	}*/

	cerr << "debug initializeRangeDecoder : decoded file : " << _inputFilename << endl;
	cerr << "debug initializeRangeDecoder : filename : " << _inputFilename << endl;
	_descInputFile = new ifstream(_inputFilename.c_str(), ios::in|ios::binary);
	//Go to the end of the file to decode blocks informations, data are read in reversed order (from right to left in the file)
	//The first number is the number of data blocks
	_descInputFile->seekg(0, _descInputFile->end);
	_inputFile = new ifstream(_inputFilename.c_str(), ios::in|ios::binary);
	
	
	if ( (_inputFile->rdstate() & std::ifstream::failbit ) != 0 )
	{
		fprintf(stderr,"cannot open file %s\n",_inputFilename.c_str());
		exit( EXIT_FAILURE);
	}

	_rangeDecoder.setInputFile(_descInputFile, true); 

}

void Requests::clearRangeDecoder(){

	_rangeDecoder.clear();
	//test
	_generalModel.clear();
	_anchorDictModel.clear();
	_numericModel.clear();
	_nbReadsPerAnchorModel.clear();
	for(int i=0; i<CompressionUtils::NB_MODELS_PER_NUMERIC; i++){
		_numericModel.push_back(Order0Model(256));
		_nbReadsPerAnchorModel.push_back(Order0Model(256));
	}
	//test
	delete _descInputFile;
	delete _inputFile;
}

void Requests::decodeInfos(){

	u_int8_t infoByte = _rangeDecoder.nextByte(_generalModel);
	//the first bit holds the file format. 0: fastq, 1: fasta
	_isFasta = ((infoByte & 0x01) == 0x01);
	//Second bit : option no header
	_noHeader = ((infoByte & 0x02) == 0x02);

	_kmerSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);

	size_t version_major = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	size_t version_minor = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	size_t version_patch = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
}

void Requests::headerSetUp(){

	///////// header setup  /////////

	string firstHeader;
	if(! _noHeader)
	{	
	//Decode the first header
	u_int16_t firstHeaderSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	for(int i=0; i<firstHeaderSize; i++){
		firstHeader += _rangeDecoder.nextByte(_generalModel);
	}
	setupNextComponent(_headerBlockSizes);
	
	}
}

void Requests::dnaSetUp(){

	/////// dna setup ////////////

	//need to init _filePosDna here
	_filePosDna = 0;

	//cerr << "Requests::dnaSetUp() - _headerBlockSizes : " << _headerBlockSizes.size() << endl;
	for(int i=0; i<_headerBlockSizes.size(); i+=2 )
	{
		//cerr << "Requests::dnaSetUp() - _headerBlockSize : " << _headerBlockSizes[i] << endl;
		_filePosDna += _headerBlockSizes[i];
	}
	
	setupNextComponent(_dnaBlockSizes);
}

void Requests::initializeDecoders(){
	if(! _noHeader)
	{
	_hdecoder = new HeaderDecoder(_leon, this, _inputFilename);
	}
	if(! _isFasta)
	{
		cout << " - testPrintReads - temporarily not treating fastq" << endl;
		//QualDecoder* _qdecoder = new QualDecoder(this, _FileQualname);
		//qualdecoders.push_back(qd);
	}
	_ddecoder = new DnaDecoder(_leon, this, _inputFilename);

}

void Requests::clearDecoders(){

	delete _ddecoder;
	if (! _noHeader){
		delete _hdecoder;
	}
	if (! _isFasta){
		delete _qdecoder;
	}
}

void Requests::headerDecoderSetup(int blockIndice){

	u_int64_t blockSize;
	int sequenceCount;
	blockSize = _headerBlockSizes[blockIndice];
	//cerr << "debug - testPrintReads - header BlockSize : " << blockSize << endl;
	sequenceCount = _headerBlockSizes[blockIndice+1];
	//hdecoder = headerdecoders[j];
	_hdecoder->setup(_filePosHeader, blockSize, sequenceCount);
	_filePosHeader += blockSize;
}

void Requests::dnaDecoderSetup(int blockIndice){

	u_int64_t blockSize;
	int sequenceCount;
	blockSize = _dnaBlockSizes[blockIndice];
	sequenceCount = _dnaBlockSizes[blockIndice+1];
	_ddecoder->setup(_filePosDna, blockSize, sequenceCount);
	_filePosDna += blockSize;

}

void Requests::qualDecoderSetup(int blockIndice){

			if(! _isFasta)
		{
			cout << "testPrintReads - fastq not treated temporarily" << endl;
				//blockSize = _qualBlockSizes[i];
				//sequenceCount = _qualBlockSizes[i+1];
				//qdecoder = qualdecoders[j];
				//qdecoder->setup(_filePosQual, blockSize, sequenceCount);
				//_filePosQual += blockSize;
		}
		else
		{
				//qdecoder= NULL;
		}
}

//~~copy of Leon private method...
void Requests::setupNextComponent(vector<u_int64_t> & blockSizes){
			//cerr << "Requests::setupNextComponent -  _inputFile eof : " << _inputFile->eof() << endl;
	//cerr << "Requests::setupNextComponent -  _inputFile bad : " << _inputFile->bad() << endl;
	//cerr << "Requests::setupNextComponent -  _inputFile fail : " << _inputFile->fail() << endl;
	//Go to the data block position (position 0 for headers, position |headers data| for reads)
	_inputFile->seekg(_filePos, _inputFile->beg);
	//cerr << "filepos : " << _filePos << endl;
	//cerr << "Requests::setupNextComponent -  _inputFile eof : " << _inputFile->eof() << endl;
	//cerr << "Requests::setupNextComponent -  _inputFile bad : " << _inputFile->bad() << endl;
	//cerr << "Requests::setupNextComponent -  _inputFile fail : " << _inputFile->fail() << endl;

	blockSizes.clear();
	
	_blockCount = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	//cerr << "Requests::setupNextComponent - _blockCount : " << _blockCount << endl;
	for(int i=0; i<_blockCount; i++){
		u_int64_t blockSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
		blockSizes.push_back(blockSize);
		//cerr << "Requests::setupNextComponent - blockSize : " << blockSize << endl;
	}
}
void Requests::decodeBloom(){
	#ifdef PRINT_DEBUG_DECODER
		cout << "\tDecode bloom filter" << endl;
	#endif
	u_int64_t total_header_block_size = 0 ;
	for(int ii=0; ii<_headerBlockSizes.size(); ii+=2 )
	{
		total_header_block_size  += _headerBlockSizes[ii];
	}
	u_int64_t bloomPos =  total_header_block_size ;
	for(int i=0; i<_dnaBlockSizes.size(); i++){
		bloomPos += _dnaBlockSizes[i];
		i += 1;
	}
	_inputFile->seekg(bloomPos, _inputFile->beg);
}

void Requests::decodeAnchorDict(){
	#ifdef PRINT_DEBUG_DECODER
		cout << "\tDecode anchor dict" << endl;
	#endif

	u_int64_t anchorDictSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	//cerr << "Requests::decodeAnchorDict - decode dict size : " << anchorDictSize << endl;
	u_int64_t anchorCount = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	//cerr << "Requests::decodeAnchorDict - decode anc count : " << anchorCount << endl;
	_anchorRangeDecoder.setInputFile(_inputFile);
	string anchorKmer = "";
	u_int64_t dictPos = _inputFile->tellg();
	//cerr << "Requests::decodeAnchorDict - dictPos : " << dictPos << endl;
	u_int64_t currentAnchorCount = 0;
	//return;
	while(currentAnchorCount < anchorCount){

		u_int8_t c = _anchorRangeDecoder.nextByte(_anchorDictModel);
		//cerr << "Leon::decodeAnchorDict - decode : " << (uint32_t) c << endl;
		//cerr << "Leon::decodeAnchorDict - decode Leon::bin2nt(c) : " << (char) Leon::bin2nt(c) << endl;
		anchorKmer += Leon::bin2nt(c); //convert to char
		if(anchorKmer.size() == _kmerSize){

			kmer_type kmer = _kmerModel->codeSeed(anchorKmer.c_str(), Data::ASCII).value() ; //then convert to bin
			_vecAnchorKmers.push_back(kmer);
			anchorKmer.clear();
			currentAnchorCount += 1;
			//cerr << "\tRequests::decodeAnchorDict() - anchor : " << kmer.toString(_kmerSize) << endl;

		}
	}
}

void Requests::decodeSortedAnchorDict(){
	#ifdef PRINT_DEBUG_DECODER
		cout << "\tDecode anchor dict" << endl;
	#endif
	cerr << "\tRequests::decodeSortedAnchorDict() - begin" << endl;
	cerr << "\tRequests::decodeSortedAnchorDict() - _filePos : " << _filePos << endl;
	cerr << "\tRequests::decodeSortedAnchorDict() - _inputFile->tellg() : " << _inputFile->tellg() << endl;
	u_int64_t anchorDictSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	cerr << "\tRequests::decodeSortedAnchorDict() - after decoding anchor DictSize : " << anchorDictSize << endl;
	u_int64_t anchorCount = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	cerr << "\tRequests::decodeSortedAnchorDict() - after decoding anchor count : " << anchorCount << endl;
		cerr << "\tRequests::decodeSortedAnchorDict() - _inputFile->tellg() : " << _inputFile->tellg() << endl;
	
	cerr << "Requests::decodeSortedAnchorDict() - before setInputFile _inputFile eof : " << _inputFile->eof() << endl;
	cerr << "Requests::decodeSortedAnchorDict() - before setInputFile _inputFile bad : " << _inputFile->bad() << endl;
	cerr << "Requests::decodeSortedAnchorDict() - before setInputFile _inputFile fail : " << _inputFile->fail() << endl;
	cerr << "Requests::decodeSortedAnchorDict() - before setInputFile _inputFile is_open : " << _inputFile->is_open() << endl;

	_anchorRangeDecoder.setInputFile(_inputFile);
	 cerr << "Requests::decodeSortedAnchorDict() - after setInputFile _inputFile eof : " << _inputFile->eof() << endl;
	 cerr << "Requests::decodeSortedAnchorDict() - after setInputFile _inputFile bad : " << _inputFile->bad() << endl;
	 cerr << "Requests::decodeSortedAnchorDict() - after setInputFile _inputFile fail : " << _inputFile->fail() << endl;
	string anchorKmer = "";
	//return;
	u_int64_t dictPos = _inputFile->tellg();
	 cerr << "Requests::decodeSortedAnchorDict() - after _inputFile->tellg() _inputFile eof : " << _inputFile->eof() << endl;
	 cerr << "Requests::decodeSortedAnchorDict() - after _inputFile->tellg() _inputFile bad : " << _inputFile->bad() << endl;
	 cerr << "Requests::decodeSortedAnchorDict() - after _inputFile->tellg() _inputFile fail : " << _inputFile->fail() << endl;
	cerr << "Requests::decodeSortedAnchorDict() - dictPos : " << dictPos << endl;
	u_int64_t currentAnchorCount = 0;

	kmer_type anchor;
	u_int64_t nbcreated ;
	_anchorKmersSortedD = new Hash16<kmer_type, u_int32_t > (anchorCount, &nbcreated );
	cerr << "\tRequests::decodeSortedAnchorDict() - after initialisation" << endl;
	
	while(currentAnchorCount < anchorCount){


		u_int8_t c = _anchorRangeDecoder.nextByte(_anchorDictModel);
		//cerr << "Requests::decodeSortedAnchorDict - decode : " << (uint32_t) c << endl;
		//cerr << "Requests::decodeSortedAnchorDict - decode Leon::bin2nt(c) : " << (char) Leon::bin2nt(c) << endl;
		anchorKmer += Leon::bin2nt(c); //convert to char

		if(anchorKmer.size() == _kmerSize){
			
			anchor = _kmerModel->codeSeed(anchorKmer.c_str(), Data::ASCII).value() ; //then convert to bin
			//cerr << "\tRequests::decodeSortedAnchorDict() - anchor : " << anchor.toString(_kmerSize) << endl;

			u_int64_t nbReads = CompressionUtils::decodeNumeric(_anchorRangeDecoder, _nbReadsPerAnchorModel);
			//cerr << "\tRequests::decodeSortedAnchorDict() - nbReads : " << nbReads << endl;

			//_vecAnchorKmers.push_back(kmer);
			_anchorKmersSortedD->insert(anchor, nbReads);

			anchorKmer.clear();

			currentAnchorCount += 1;
		}
	}
	
	#ifdef PRINT_DEBUG_DECODER
		cout << "\t\tAnchor count: " << _vecAnchorKmers.size() << endl;
	#endif
}

/*****query*****/

void Requests::fgetRequests(){

	char kmer_req[_kmerSize+3];
	char sequence_req[sequenceMaxSize+3];

	do{

	cout << endl << endl <<
		"############## DEBUG - TESTS #############" << endl << endl <<
		"t sig\t\t\tto print sinatures" << endl <<
		"t col\t\t\tto print colors" << endl <<
		"t seq\t\t\tto print sequences" << endl <<
		"t kmers\t\t\tto print kmers" << endl <<
		"t mphf\t\t\tto print mphf indexes" << endl << 
		"t seq anchors\t\tto print sequence's anchors" << endl << 
		"t seq anchors dict\tto print the dictionnary's sequence's anchors" << endl << 
		"t anchors dict sd\tto print the dictionnary's sequence's anchors sorted" << endl  <<
		"testall\t\t\tto print kmers, indexes in mphf, color and signature" << endl << endl <<

		"~~~~~~~ LEON ~ MODE ~~~~~~" << endl << endl <<

		"t read canchors\t\tto print compressed reads' anchors in file order" << endl << 
		"t read canchors pos\tto print compressed reads' anchors' positions in file order" << endl << 
		"t read creads\t\tto print compressed reads in file order" << endl << 
		"t read c-all\t\tto print all three above informations in file order" << endl << 
		"t read cfile\t\tto print original compressed file" << endl <<  endl <<

		"~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl <<

		"~~~~~ PEACOCK ~ MODE ~~~~~" << endl << endl <<

		"t read panchors\t\tto print peacock reads' anchors in file order" << endl << 
		"t read panchors pos\tto print peacock reads' anchors' positions in file order" << endl << 
		"t read preads\t\tto print peacock reads in file order" << endl << 
		"t read p-all\t\tto print all three above informations in file order" << endl << 
		"t read pfile\t\tto print original peacock file" << endl << endl <<

		"~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl <<

		"##########################################" << endl << endl <<

		"################ REQUESTS ################" << endl << endl <<
		
		"nb ds \t\t\tto get the number of datasets in the file" << endl << endl <<
		
		"kmer s \t\t\tto get size of kmers" << endl <<
		"kmer p \t\t\tto know if the kmer is present in the data" << endl <<
		"kmer h \t\t\tto know in how many datasets the kmer is present" << endl <<
		"kmer d \t\t\tto know in which datasets the kmer is present" << endl << endl <<

		"seq pg\t\t\tto know if the sequence is present in the graph" << endl <<
		"seq hg\t\t\tto know in the sequence's colors' number in the graph" << endl <<
		"seq dg\t\t\tto know in which datasets the sequence is present in the graph" << endl << endl <<

		//"~~~~~~~ LEON ~ MODE ~~~~~~" << endl << endl <<

		"seq p\t\t\tto know if the sequence is present in the data" << endl <<
		"seq h\t\t\tto know in the sequence's colors' number in the data" << endl <<
		"seq d\t\t\tto know in which datasets the sequence is present in the data" << endl <<
		"seq m\t\t\tto get the sequence's matches with data" << endl << endl << 

		//"~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl <<

		//"~~~~~ PEACOCK ~ MODE ~~~~~" << endl << endl <<

		//"~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl <<

		"q \t\tto quit" << endl << endl <<

		"##########################################" << endl << endl;

		fgets(request, req_buffer_size, stdin);
		request[strlen(request)-1]='\0';

		//cout << req << " strlen req : " << strlen(req) << endl;
		cout << endl;

		//debug
		if (strcmp(request, "t sig")==0){
			this->printSignatures();
		}

		if (strcmp(request, "t col")==0){
			this->printColors();
		}

		if (strcmp(request, "t seq")==0){

			cout << "not available anymore : requests now separated from compression" << endl;
			//this->printSequences();
		}

		if (strcmp(request, "t kmers")==0){
			this->printKmers();
		}

		if (strcmp(request, "t mphf")==0){
			this->printMPHFIndexes();
		}

		if (strcmp(request, "t seq anchors")==0){
			if (this->fgetSequence(sequence_req)){
				this->printSequenceAnchors(sequence_req);	
			}
		}

		if (strcmp(request, "t seq anchors dict")==0){
			if (this->fgetSequence(sequence_req)){

				u_int64_t dictSize = strlen(sequence_req);
				u_int64_t nbcreated;
				Hash16<kmer_type, list<u_int32_t>*>* sequenceAnchorKmers = new Hash16<kmer_type, list<u_int32_t>*>(dictSize , &nbcreated);
				fillSequenceAnchorsDict(sequenceAnchorKmers, sequence_req);

				this->printSequenceAnchorsDict(sequence_req, sequenceAnchorKmers);	
			
				cout << "enter any kmer to test if in dictionnary" << endl;
				cout << "enter q to quit" << endl;

				char kmer_chars[1024];
				while(strcmp(kmer_chars, "q")!=0){
					if(this->fgetKmer(kmer_chars)){
						printIsKmerInSequenceAnchorDict(kmer_chars, sequenceAnchorKmers);
					}
				}
				emptySequenceAnchorDict(sequenceAnchorKmers, sequence_req);
			}

		}

		if (strcmp(request, "t anchors dict sd")==0){
			
			cout << "enter any kmer to test if in sorted dictionnary" << endl;
			cout << "enter q to quit" << endl;

			char kmer_chars[1024];
			kmer_type anchor;
			u_int32_t nbReads; 
			while(strcmp(kmer_chars, "q")!=0){
				if(this->fgetKmer(kmer_chars)){
					anchor = getKmerType(kmer_chars);
					
					if(_anchorKmersSorted->get(anchor, &nbReads)){
						cout << "present, nbReads for this anchor : " << nbReads << endl;
					}
					else{
						cout << "not present" << endl;
					}
				}
			}
		}

		if (strcmp(request, "testall")==0){
			this->printTestAll();
		}

		//leon mode

		if (strcmp(request, "t read canchors")==0){
			if (! _orderReads){
				this->testPrintReadsFile(false, true, false);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read canchors pos")==0){
			if (! _orderReads){
				this->testPrintReadsFile(false, false, true);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read creads")==0){
			if (! _orderReads){
				this->testPrintReadsFile(true, false, false);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read c-all")==0){
			if (! _orderReads){
				this->testPrintReadsFile(true, true, true);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read cfile")==0){
			if (! _orderReads){
				this->testPrintAllHeadersReadsFile();
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		//peacock mode

		if (strcmp(request, "t read panchors")==0){
			if (_orderReads){
				this->testPrintReadsPFile(false, true, false);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read panchors pos")==0){
			if (_orderReads){
				this->testPrintReadsPFile(false, false, true);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read preads")==0){
			if (_orderReads){
				this->testPrintReadsPFile(true, false, false);
			}
			else{
				cout << "available only in leon mode" << endl;
			}
		}

		if (strcmp(request, "t read p-all")==0){
			if (_orderReads){
				this->testPrintReadsPFile(true, true, true);
			}
			else{
				cout << "available only in peacock mode" << endl;
			}
		}

		if (strcmp(request, "t read pfile")==0){
			if (_orderReads){
				this->testPrintPFile();
			}
			else{
				cout << "available only in peacock mode" << endl;
			}
		}

		//requests


		if (strcmp(request, "nb ds")==0){
			//this->printNbBanks();
			cout << "unavailable for now... find another way to get nb of datasets" << endl;
		}

		if (strcmp(request, "kmer s")==0){
			this->printKmerSize();
		}

		if (strcmp(request, "kmer p")==0){

			if(this->fgetKmer(kmer_req)){

				if (this->isKmerInGraph(kmer_req)){
					std::cout << kmer_req << endl <<
					" is present in data" << std::endl;
				}
				else{
					std::cout << kmer_req << endl <<
					" is not present in data" << std::endl;
				}
			}
		}

		if (strcmp(request, "kmer h")==0){

			if (this->fgetKmer(kmer_req)){
				int nbData = this->getKmerNbColors(kmer_req);
				cout << nbData << endl;
			}
		}

		if (strcmp(request, "kmer d")==0){

			if (this->fgetKmer(kmer_req)){
				bitset<NB_MAX_COLORS> kmer_colors = this->getKmerColors(kmer_req);

				if (kmer_colors.none()){
					cout <<  kmer_req << endl <<
					" is not present in any dataset" << endl;
				}

				else{
					//TODO get nb data sets to minimize the loop on NB_MAX COLORS

					cout <<  kmer_req << endl <<
					" is present in the following dataset : " << endl;
					for (int i=0; i<NB_MAX_COLORS; ++i){
						if (kmer_colors.test(i)){
							cout << i << endl;
						}
					}
				}
			}
		}

		if (strcmp(request, "seq pg")==0){

			if (this->fgetSequence(sequence_req))
			{
				if(this->isSequenceInGraph(sequence_req))
				{
					std::cout << sequence_req << endl <<
					" is present in graph" << std::endl;
				}
				else{
					std::cout << sequence_req << endl <<
					" is not present in graph" << std::endl;
				}
			}
			
		}

		if (strcmp(request, "seq hg")==0){

			if (this->fgetSequence(sequence_req)){
				int nbData = this->getSequenceNbColorsInGraph(sequence_req);
				cout << nbData << endl;
			}
		}

		if (strcmp(request, "seq dg")==0){

			if (this->fgetSequence(sequence_req)){
				bitset<NB_MAX_COLORS> sequence_colors = this->getSequenceColorsInGraph(sequence_req);

				if (sequence_colors.none()){
					cout <<  sequence_req << endl <<
					" is not present in any dataset" << endl;
				}

				else{
					//TODO get nb data sets to minimize the loop on NB_MAX COLORS

					cout <<  sequence_req << endl <<
					 " is present in the following dataset : " << endl;
					for (int i=0; i<NB_MAX_COLORS; ++i){
						if (sequence_colors.test(i)){
							cout << i << endl;
						}
					}
				}
			}
		}

		if (strcmp(request, "seq p")==0)
		{
			if (this->fgetSequence(sequence_req))
			{
				int sequenceSize = strlen(sequence_req);
				vector<bitset<NB_MAX_COLORS>>* sequenceMatches = new vector<bitset<NB_MAX_COLORS>>(sequenceSize,bitset<NB_MAX_COLORS>()); 
				//bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches/*(std::string("11111110"))*/;

				//if (!_orderReads){

				if (this->isSequenceInData(sequence_req, sequenceMatches/*, _sequenceAmbiguousMatches*/))
				{
					std::cout << sequence_req << endl << 
					" is present in data" << std::endl;

					if (_sequenceAmbiguousMatches.any()){
						cout << endl << "warning, some colors may have been fullfilled using the same read at least twice : " <<
						endl << _sequenceAmbiguousMatches << endl << endl;
					}
				}
				else
				{
					std::cout << sequence_req << endl <<
					 " is not present in data" << std::endl;
				}
				//}
				//else{
				//	cout << endl << "not yet available on peacock mode" << endl;
				//}
				delete sequenceMatches;
			}
		}

		if (strcmp(request, "seq h")==0)
		{
			if (this->fgetSequence(sequence_req))
			{
				int sequenceSize = strlen(sequence_req);
				vector<bitset<NB_MAX_COLORS>>* sequenceMatches = new vector<bitset<NB_MAX_COLORS>>(sequenceSize,bitset<NB_MAX_COLORS>()); 
				//bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches;

				//if (!_orderReads){

				int nbData = this->getSequenceNbColorsInData(sequence_req, sequenceMatches/*, _sequenceAmbiguousMatches*/);
				cout << nbData << endl;

				if (_sequenceAmbiguousMatches.any()){
					cout << endl << "warning, some colors may have been fullfilled using the same read at least twice : " <<
					endl << _sequenceAmbiguousMatches << endl << endl;
				}
				
				//}
				//else{
				//	cout << endl << "not yet available on peacock mode" << endl;
				//}

				delete sequenceMatches;
			}
		}

		if (strcmp(request, "seq d")==0)
		{
			if (this->fgetSequence(sequence_req))
			{
				int sequenceSize = strlen(sequence_req);
				vector<bitset<NB_MAX_COLORS>>* sequenceMatches = new vector<bitset<NB_MAX_COLORS>>(sequenceSize,bitset<NB_MAX_COLORS>()); 
				//bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches;

				//if (!_orderReads){

				bitset<NB_MAX_COLORS> sequence_colors = this->getSequenceColorsInData(sequence_req, sequenceMatches/*, _sequenceAmbiguousMatches*/);

				if (sequence_colors.none())
				{
					cout <<  sequence_req << endl <<
					" is not present in any dataset" << endl;
				}

				else
				{
						//TODO get nb data sets to minimize the loop on NB_MAX COLORS

					cout <<  sequence_req << endl <<
					 " is present in the following dataset : " << endl;
					for (int i=0; i<NB_MAX_COLORS; ++i)
					{
						if (sequence_colors.test(i))
						{
							cout << i << endl;
						}
					}
					if (_sequenceAmbiguousMatches.any()){
						cout << endl << "warning, some colors may have been fullfilled using the same read at least twice : " <<
						endl << _sequenceAmbiguousMatches << endl << endl;
					}
				}
				//}
				//else{
				//	cout << endl << "not yet available on peacock mode" << endl;
				//}

				delete sequenceMatches;
			}
			
		}

		if (strcmp(request, "seq m")==0){
			if (this->fgetSequence(sequence_req)){

				int sequenceSize = strlen(sequence_req);
				vector<bitset<NB_MAX_COLORS>>* sequenceMatches = new vector<bitset<NB_MAX_COLORS>>(sequenceSize,bitset<NB_MAX_COLORS>()); 
				//bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches;

				//if (!_orderReads){

					getSequenceFileMatchesInData(sequence_req, sequenceMatches/*, _sequenceAmbiguousMatches*/);	

					//tmp print
					for (int i = 0; i < sequenceSize; ++i)
					{
						cout << sequence_req[i] << " : " << bitset<NB_MAX_COLORS>((*sequenceMatches)[i]) << endl;
					}

					if (_sequenceAmbiguousMatches.any()){
						cout << endl << "warning, some colors may have been filled using the same read at least twice : " <<
						endl << _sequenceAmbiguousMatches << endl << endl;
					}
				//}
				//else {
				//	cout << endl << "not yet available on peacock mode" << endl;
				//}

				delete sequenceMatches;
			}
		}

		if (strcmp(request, "q")==0)
		{
			this->end_requests = true;
		}

	}while(!this->end_requests);

}

void Requests::fgetString(char* string, int stringLen, char* query){

	std::cout << query << std::endl << std::endl;
	fgets(string, stringLen, stdin);

	if (string[strlen(string)-1] != '\n'){ //flush buffer
		int c;
		while(((c = fgetc(stdin)) != '\n') && (c != EOF));
	}
	string[strlen(string)-1]='\0';
}

bool Requests::fgetKmer(char* kmer){

	this->fgetString(kmer, _kmerSize+3, "enter the kmer : "); 
	//+3 for \n \0 and if the string is longer than kmer_size
	//we need to store at least one char to know it	

	if (strlen(kmer) != _kmerSize){
		cout << "error : the size of kmer is " << _kmerSize << endl;
		return false;
	}

	return true;
}

bool Requests::fgetSequence(char* sequence){

	this->fgetString(sequence, sequenceMaxSize+3, "enter the sequence : "); 
	//+3 for \n \0 and if the string is longer than sequenceMaxSize
	//we need to store at least one char to know it	

	if (strlen(sequence) > sequenceMaxSize){
		cout << "error : the size of the sequence cannot exceed " << sequenceMaxSize << endl;
		return false;
	}

	if (strlen(sequence) < _kmerSize){
		cout << "error : the size of the sequence cannot be smaller than " << 
		_kmerSize << " (the kmer's size)" << endl;
		return false;
	}

	return true;
}

/*****debug*****/

void Requests::printSignatures(){
	cout << "test signatures" << endl;

	cout << "signatures : \n" << endl;

	for (int i=0; i < _nb_kmers_infile; ++i){
		cout << std::bitset<NB_MAX_COLORS>(_signature_array[i]) << endl;
	}
}

void Requests::printColors(){
	cout << "test signatures" << endl;

	cout << "colors : \n" << endl;

	for (int i=0; i < _nb_kmers_infile; ++i){
		cout << std::bitset<NB_MAX_COLORS>(_color_array[i]) << endl;
	}
}

void Requests::printKmers(){
for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next()){
			
	std::cout <<  _model.toString (_itKmers->item().getValue())  << endl;
	}

}

//used for tests, delete later
/*
void Requests::printSequences(){

    for (_itBank->first(); !_itBank->isDone(); _itBank->next())
	{
		
		std::cout <<  _itBank->item().toString()  << endl << endl;

	}
}
*/

void Requests::printMPHFIndexes(){
for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next()){
			
	Node node(Node::Value(_itKmers->item().getValue()));
		printf("_graph.nodeMPHFIndex(node) : %d\n", _graph.nodeMPHFIndex(node));
	}

}

void Requests::printSequenceAnchors(char* sequence){

	u_int32_t anchorAddress;
	uint nbKmer = 0;
	char kmer[_kmerSize+1];
	char anchor[_kmerSize+1];

	cout << "anchor kmers : " << endl;
	
	while(getSequenceKmer(sequence, nbKmer, kmer))
	{	

		//cout << "anchor tested : " << kmer << endl;
		if (getNextAnchor(sequence, &nbKmer, anchor, anchorAddress)){
			cout << "anchor : " << anchor << endl;
		}

		nbKmer++;
	}
}

void Requests::printIsKmerInSequenceAnchorDict(char* kmer_chars, 
				Hash16<kmer_type, list<u_int32_t>* >* sequenceAnchorKmers){
	
	list<u_int32_t>* listPos;

	kmer_type kmer = getKmerType(kmer_chars);
	if (sequenceAnchorKmers->get(kmer, &listPos)){
		cout << "yes, at position(s) :" << endl;

		for (std::list<u_int32_t>::iterator it=listPos->begin(); it != listPos->end(); ++it){
			cout << *it << endl;
		}
	}
	else{
		cout << "no" << endl;
	}
}

//leon mode

void Requests::printSequenceAnchorsDict(char* sequence, 
								Hash16<kmer_type, list<u_int32_t>* >* sequenceAnchorKmers){


	u_int32_t anchorAddress;
	//char anchor_chars[_kmerSize+1];
	uint nbKmer = 0;
	char kmer[_kmerSize+1];
	char anchor[_kmerSize+1];
	//cerr << "Requests::printSequenceAnchorsDict - test";
	while(getSequenceKmer(sequence, nbKmer, kmer))
	{	
		if (getNextAnchor(sequence, &nbKmer, anchor, anchorAddress)){
			cout << "anchor : " << anchor << endl;
			cout << "test if in dictionnary : " << endl;

			printIsKmerInSequenceAnchorDict(anchor, sequenceAnchorKmers);
		}

		nbKmer++;
	}
}


void Requests::printTestAll(){

 for (_itKmers->first(); !_itKmers->isDone(); _itKmers->next())
	{
		
		
	Node node(Node::Value(_itKmers->item().getValue()));
	printf("_graph.nodeMPHFIndex(node) %d\n", _graph.nodeMPHFIndex(node));
		
	std::cout <<  _model.toString (_itKmers->item().getValue())  << "\t" <<
	std::bitset<NB_MAX_COLORS>(_color_array[_graph.nodeMPHFIndex(node)]) << "\t" <<
	std::bitset<NB_MAX_COLORS>(_signature_array[_graph.nodeMPHFIndex(node)]) << std::endl;

	} 
}



void Requests::testPrintReadsFile(bool getReads, bool getAnchors, bool getAnchorPos){

	_filePos = 0;
	u_int64_t filePosHeader = 0;
	u_int64_t filePosDna = 0;
	initializeRangeDecoder();

	decodeInfos();
	headerSetUp();
	dnaSetUp();
	
	decodeBloom();
	decodeAnchorDict();

	initializeDecoders();

	//cerr << "debug Requests::testPrintReadsFile - reading blocks : " << endl;
	//cerr << "debug Requests::testPrintReadsFile - _dnaBlockSizes.size() : " << _dnaBlockSizes.size() << endl;

	for (int blockIndice = 0; 
		blockIndice < _dnaBlockSizes.size(); 
		blockIndice += 2){
			
		//cerr << "debug Requests::testPrintReadsFile - block nb : " << blockIndice << endl;

		//if(blockIndice >= _dnaBlockSizes.size()) break;
			
		if(! _noHeader)
		{
			headerDecoderSetup(blockIndice);
		}	
		dnaDecoderSetup(blockIndice);
		qualDecoderSetup(blockIndice);

		//struct ReadInfos* ri = (struct ReadInfos*)malloc(sizeof(struct ReadInfos));
		struct ReadInfos* ri = new ReadInfos{};
		int nbRead = 0;
		while(_ddecoder->getNextReadInfos(ri)){
			
			cout << "element " << nbRead << endl;
			
			if (getReads){
				cout << "read type : " << (int) ri->readType << endl;
				cout << "read  : " << ri->sread;
			}
			if (getAnchors){
				cout << "anchor : " << getKmerString(ri->anchor) << endl;
				cout << "reversed : " << ri->revcomp << endl;
				if (ri->revcomp){
					cout << "reversed anchor : " << getKmerString(ri->revAnchor) << endl;
				}
			}
			if (getAnchorPos){
				cout << "anchorPos : " << ri->anchorPos << endl;
			}
			cout << endl;
			++nbRead;
		}
	}	


	clearRangeDecoder();
	clearDecoders();


}

void Requests::testPrintAllHeadersReadsFile(){

	_filePos = 0;
	u_int64_t filePosHeader = 0;
	u_int64_t filePosDna = 0;
	initializeRangeDecoder();

	decodeInfos();
	headerSetUp();
	dnaSetUp();
	
	decodeBloom();
	decodeAnchorDict();
	
	initializeDecoders();

	int i=0;
	//cerr << "debug - testPrintReads - _dnaBlockSizes : " << _dnaBlockSizes.size() << endl;
	while(i < _dnaBlockSizes.size()){
		
		cerr << "debug - testPrintReads - block nb : " << i << endl;
		//for(int j=0; j<_nb_cores; j++){
			

			//if(i >= _dnaBlockSizes.size()) break;
			
			
			u_int64_t blockSize;
			int sequenceCount;
			
			//livingThreadCount = j+1;
			
			//QualDecoder* qdecoder;
			//HeaderDecoder* hdecoder;
			//DnaDecoder* ddecoder;
			
			//header decoder
			
			if(! _noHeader)
			{
				cerr << "debug - testPrintReads - test1" << endl;
				headerDecoderSetup(i);
				cerr << "debug - testPrintReads - test2" << endl;
			}

			/*if(! _noHeader)
			{
				blockSize = _headerBlockSizes[i];
				cerr << "debug - testPrintReads - header BlockSize : " << blockSize << endl;
				sequenceCount = _headerBlockSizes[i+1];
				//hdecoder = headerdecoders[j];
				_hdecoder->setup(filePosHeader, blockSize, sequenceCount);
				filePosHeader += blockSize;
				//hdecoder->execute();
			}
			else
			{
				_hdecoder= NULL;
			}*/
			
			//dna decoder
			blockSize = _dnaBlockSizes[i];
			cerr << "debug - testPrintReads - dna BlockSize : " << blockSize << endl;
			sequenceCount = _dnaBlockSizes[i+1];
			//ddecoder = dnadecoders[j];
			_ddecoder->setup(filePosDna, blockSize, sequenceCount);
			filePosDna += blockSize;
			//ddecoder->execute();

			//qual decoder setup
			//here test if in fastq mode, put null pointer otherwise
			if(! _isFasta)
			{
				cout << "testPrintReads - fastq not treated temporarily" << endl;
				//blockSize = _qualBlockSizes[i];
				//sequenceCount = _qualBlockSizes[i+1];
				//qdecoder = qualdecoders[j];
				//qdecoder->setup(_filePosQual, blockSize, sequenceCount);
				//_filePosQual += blockSize;
			}
			else
			{
				//qdecoder= NULL;
			}

			if(_hdecoder!=NULL){
				cerr << "debug - testPrintReads - before hdecoder execute" << endl;
				_hdecoder->execute();
				cerr << "debug - testPrintReads - after hdecoder execute" << endl;
			}
			cerr << "debug - testPrintReads - before ddecoder execute" << endl;
			_ddecoder->execute();
			cerr << "debug - testPrintReads - after ddecoder execute" << endl;
			i += 2;

		}	
	

	std::istringstream  * stream_qual = NULL;
	std::istringstream  * stream_header = NULL;

	if(! _isFasta)
		{
		cout << " - testPrintReads - fastq not treated temporarily" << endl;
		//qdecoder = qualdecoders[j];
		//stream_qual = new std::istringstream (qdecoder->_buffer);
		//qdecoder->_buffer.clear();

		}
			
	if(! _noHeader)
		{
		//hdecoder = headerdecoders[j];
		stream_header = new std::istringstream (_hdecoder->_buffer);
		_hdecoder->_buffer.clear();

		}

	std::istringstream stream_dna (_ddecoder->_buffer);
			
	_ddecoder->_buffer.clear();

	std::string line;
	std::string output_buff;

			
			
	output_buff.reserve(Leon::READ_PER_BLOCK * 500);
			
	bool reading = true;
			
			
	u_int64_t readid=0;
	while(reading){
				
		stringstream sint;
		sint << readid;
				
		if( ! _noHeader)
		{
			if(getline(*stream_header, line)){
	//			cout << "debug - testPrintReads - getline" << endl;
				if(_isFasta)
					output_buff += ">";
				else
					output_buff += "@";
					
				output_buff +=  line + '\n';
			}
			else{
	//			cout << "debug - testPrintReads - getline header false" << endl;
				reading = false;
			}
		}
		else
		{
			if(_isFasta)
				output_buff += "> " + sint.str() + '\n';
			else
				output_buff += "@ " + sint.str() + '\n';
					
			readid++;
		}
				 
				
				
		if(getline(stream_dna, line)){
	//		cout << "debug - testPrintReads - getline" << endl;
			output_buff +=  line + '\n';
		}
		else{
	//			cout << "debug - testPrintReads - getline dna false" << endl;
				reading = false;
			}
				
				
		if( ! _isFasta)
		{
			cout << " - testPrintReads - fastq not treated temporarily" << endl;
			//if(getline(*stream_qual, line)){
			//	output_buff += "+\n";
			//	output_buff +=  line + '\n';
			//}
			//else
			//	reading = false;
		}
				 
	}
			
			 
	//_outputFile->fwrite(output_buff.c_str(), output_buff.size(), 1);
	cout << " - testPrintReads - buff : \n" << output_buff.c_str() << endl;

	if(stream_qual!= NULL) delete  stream_qual;
	if(stream_header!= NULL) delete  stream_header;
			
			
		
		
	//livingThreadCount = 0;

	//cout << "debug testPrintReads : before clearRangeDecoder()" << endl; 
	clearRangeDecoder();
	delete _hdecoder;
	delete _ddecoder;
	//std::cout << "debug testPrintReads : after clearRangeDecoder()" << endl; 
	//cout << "debug testPrintReads END" << endl; 

}

//peacock mode

void Requests::testPrintReadsPFile(bool getReads, bool getAnchors, bool getAnchorPos){
	
	_filePos = 0;
	u_int64_t filePosHeader = 0;
	u_int64_t filePosDna = 0;
	initializeRangeDecoder();

	decodeInfos();
	headerSetUp();
	dnaSetUp();
	
	decodeBloom();
	//decodeAnchorDict();
	decodeSortedAnchorDict();

	initializeDecoders();

	//cerr << "debug Requests::testPrintReadsPFile - reading blocks : " << endl;
	//cerr << "debug Requests::testPrintReadsPFile - _dnaBlockSizes.size() : " << _dnaBlockSizes.size() << endl;

	for (int blockIndice = 0; 
		blockIndice < _dnaBlockSizes.size(); 
		blockIndice += 2){
			
		//cerr << "debug Requests::testPrintReadsPFile - block nb : " << blockIndice << endl;

		//if(blockIndice >= _dnaBlockSizes.size()) break;
			
		if(! _noHeader)
		{
			headerDecoderSetup(blockIndice);
		}	
		dnaDecoderSetup(blockIndice);
		qualDecoderSetup(blockIndice);

		//struct ReadInfos* ri = (struct ReadInfos*)malloc(sizeof(struct ReadInfos));
		struct OrderedReadsInfosBlock* orib = new OrderedReadsInfosBlock{};
		int nbRead = 0;
		//cerr << "debug Requests::testPrintReadsPFile - before  : getNextReadsInfosBLock(orib)" << endl;

		while(_ddecoder->getNextOrderedReadsInfosBLock(orib)){

			for (int i=0; i < orib->nbReads; ++i){
			
				cout << "element " << nbRead << endl;
				struct ReadInfos* ri = new ReadInfos{};
				if (_ddecoder->getNextOrderedReadInfos(ri)){
				
					if (getReads){
						cout << "read type : " << (int) ri->readType << endl;
						cout << "read  : " << ri->sread;
					}
					if (getAnchors){
						cout << "anchor : " << getKmerString(ri->anchor) << endl;
						cout << "reversed : " << ri->revcomp << endl;
						if (ri->revcomp){
							cout << "reversed anchor : " << getKmerString(ri->revAnchor) << endl;
						}
					}
					if (getAnchorPos){
						cout << "anchorPos : " << ri->anchorPos << endl;
					}
					cout << endl;
					++nbRead;
				}
				/*else{
					cerr << "debug Requests::testPrintReadsPFile - ERROR" << endl;
				}*/
			}
		}
	}	


	clearRangeDecoder();
	clearDecoders();	
}

void Requests::testPrintPFile(){
	
	_filePos = 0;

	initializeRangeDecoder();

	//original decoding order :

	u_int8_t infoByte = _rangeDecoder.nextByte(_generalModel);
	//cout << endl << "\tinfoByte : " << bitset<8>(infoByte) << endl;

	//the first bit holds the file format. 0: fastq, 1: fasta
	bool isFasta = ((infoByte & 0x01) == 0x01);
	
	
	
	//Second bit : option no header
	bool noHeader = ((infoByte & 0x02) == 0x02);
	//cerr << "testPrintPFile - noHeader : " << noHeader << endl;

	_kmerSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	cout << "\tKmer size: " << _kmerSize << endl;

	size_t version_major = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	size_t version_minor = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
	size_t version_patch = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);

	cout << "\tversion_major: " << version_major << endl;
	cout << "\tversion_minor: " << version_minor << endl;
	cout << "\tversion_patch: " << version_patch << endl;

	cerr << "Requests::testPrintPFile() - SEG0" << endl;

	u_int64_t filePosHeader = 0;
	u_int64_t filePosDna = 0;
	string firstHeader;
	
	if(! noHeader)
	{
		
		///////// header setup  /////////
		//Decode the first header
		//cerr << "debug - testPrintPFile - header setup" << endl;
		u_int16_t firstHeaderSize = CompressionUtils::decodeNumeric(_rangeDecoder, _numericModel);
		for(int i=0; i<firstHeaderSize; i++){
			firstHeader += _rangeDecoder.nextByte(_generalModel);
			//cerr << "debug - testPrintPFile - first header : " << firstHeader << endl;
		}
		//cerr << "debug - testPrintPFile - headerBlockSizes before setup : " << _headerBlockSizes.size() << endl;
		//_filePos = filePosHeader;
		setupNextComponent(_headerBlockSizes);
		//cerr << "debug - testPrintPFile - headerBlockSizes after setup : " << _headerBlockSizes.size() << endl;
	
	}
	//MARQUEUR
		cerr << "Requests::testPrintPFile() - SEG1" << endl;
	/////// dna setup ////////////
	
	//need to init _filePosDna here
	//cerr << "debug - testPrintPFile - dna setup" << endl;
	for(int ii=0; ii<_headerBlockSizes.size(); ii+=2 )
	{
		//filePosDna += _headerBlockSizes[ii];
		//cerr << "debug - testPrintPFile - file pos dna : " << filePosDna << endl;
	}
	
	//cerr << "debug - testPrintPFile - dnaBlockSizes before setup : " << _dnaBlockSizes.size() << endl;
	_filePos = filePosDna;
	//cerr << "debug - testPrintPFile - _filePos : " << _filePos << endl;
	setupNextComponent(_dnaBlockSizes);
	//cerr << "debug - testPrintPFile - dnaBlockSizes after setup : " << _dnaBlockSizes.size() << endl;
	//cerr << "debug - testPrintPFile - _filePos : " << _filePos << endl;
	cerr << "Requests::testPrintPFile() - SEG2" << endl;

	decodeBloom();
	//cerr << "debug - testPrintPFile - _filePos after decode bloom : " << _filePos << endl;
	//decodeAnchorDict();

	//cerr << "debug - testPrintPFile - read no anchored reads here ?" << endl;

	/*u_int64_t nbReadsNoAnchor = CompressionUtils::decodeNumeric(_rangeEncoder, _numericModel);
	cerr << "debug - testPrintPFile - nbReadsNoAnchor : " << nbReadsNoAnchor << endl;
	exit(EXIT_FAILURE);

	for (int i=0; i<nbReadsNoAnchor)
	{
		de->encodeSortedFileNoAnchorRead(line);
	}*/
	cerr << "Requests::testPrintPFile() - SEG3" << endl;
	decodeSortedAnchorDict();
	cerr << "Requests::testPrintPFile() - SEG4" << endl;
		/////////// qualities setup //////////
	/*if(! isFasta)
	{
	_filePosQual =0;
	
	//read block sizes and _blockCount
	_qualBlockSizes.clear();
	_inputFileQual->seekg(- sizeof(u_int64_t),_inputFileQual->end);
	
	_inputFileQual->read((char *)&_blockCount,sizeof(u_int64_t));
	//cout << "\tBlock count: " << _blockCount/2 << endl;
	
	_qualBlockSizes.resize(_blockCount,0);
	char * databuff = (char * )& _qualBlockSizes[0];
	
	_inputFileQual->seekg(- (sizeof(u_int64_t)*(_blockCount+1)),_inputFileQual->end);
	_inputFileQual->read( databuff ,sizeof(u_int64_t) *  _blockCount);
	
	}*/
	
	//QualDecoder* qdecoder;
	HeaderDecoder* hdecoder;
	DnaDecoder* ddecoder;

	if(! isFasta)
	{
		cout << " - testPrintPFile - temporarily not treating fastq" << endl;
		//QualDecoder* qd = new QualDecoder(this, _FileQualname);
		//qualdecoders.push_back(qd);
	}
		
	//DnaDecoder* dd = new DnaDecoder(_leon, _outputFilename);
	//cerr << " debug - testPrintPFile - decodeFileName : " << _decodeFilename << endl;
	ddecoder = new DnaDecoder(_leon, this, _inputFilename);
	//dnadecoders.push_back(dd);
		
	if(! noHeader)
	{
	//HeaderDecoder* hd = new HeaderDecoder(_leon, _outputFilename);
	hdecoder = new HeaderDecoder(_leon, this, _inputFilename);
	//headerdecoders.push_back(hd);
	}

	cerr << "Requests::testPrintPFile() - SEG5" << endl;
	int i=0;
	cerr << "debug - testPrintPFile - _dnaBlockSizes : " << _dnaBlockSizes.size() << endl;
	while(i < _dnaBlockSizes.size())
	{
		
		cerr << "debug - testPrintPFile - block nb : " << i << endl;
		//for(int j=0; j<_nb_cores; j++){
			

			//if(i >= _dnaBlockSizes.size()) break;
			
			
			u_int64_t blockSize;
			int sequenceCount;
			
			//livingThreadCount = j+1;
			
			//QualDecoder* qdecoder;
			//HeaderDecoder* hdecoder;
			//DnaDecoder* ddecoder;
			
			//header decoder
			if(! noHeader)
			{
				blockSize = _headerBlockSizes[i];
				//cerr << "debug - testPrintPFile - header BlockSize : " << blockSize << endl;
				sequenceCount = _headerBlockSizes[i+1];
				//hdecoder = headerdecoders[j];
				hdecoder->setup(filePosHeader, blockSize, sequenceCount);
				filePosHeader += blockSize;
				
				//hdecoder->execute();
			}
			else
			{
				hdecoder= NULL;
			}
			
			//dna decoder
			blockSize = _dnaBlockSizes[i];
			cerr << "debug - testPrintPFile - dna BlockSize : " << blockSize << endl;
			sequenceCount = _dnaBlockSizes[i+1];
			//ddecoder = dnadecoders[j];
			ddecoder->setup(filePosDna, blockSize, sequenceCount);
			filePosDna += blockSize;
			//ddecoder->execute();

			//qual decoder setup
			//here test if in fastq mode, put null pointer otherwise
			if(! isFasta)
			{
				cout << "testPrintPFile - fastq not treated temporarily" << endl;
				//blockSize = _qualBlockSizes[i];
				//sequenceCount = _qualBlockSizes[i+1];
				//qdecoder = qualdecoders[j];
				//qdecoder->setup(_filePosQual, blockSize, sequenceCount);
				//_filePosQual += blockSize;
			}
			else
			{
				//qdecoder= NULL;
			}

			if(hdecoder!=NULL){
				cerr << "debug - testPrintPFile - before hdecoder execute" << endl;
				hdecoder->execute();
				cerr << "debug - testPrintPFile - after hdecoder execute" << endl;
			}
			cerr << "debug - testPrintPFile - before dnadecoder execute" << endl;
			ddecoder->execute();
			cerr << "debug - testPrintPFile - after dnadecoder execute" << endl;
			i += 2;

		}	
	
	cerr << "Requests::testPrintPFile() - SEG6" << endl;

	std::istringstream  * stream_qual = NULL;
	std::istringstream  * stream_header = NULL;

	if(! isFasta)
		{
		cout << " - testPrintPFile - fastq not treated temporarily" << endl;
		//qdecoder = qualdecoders[j];
		//stream_qual = new std::istringstream (qdecoder->_buffer);
		//qdecoder->_buffer.clear();

		}
			
	if(! noHeader)
	{
		//hdecoder = headerdecoders[j];
		stream_header = new std::istringstream (hdecoder->_buffer);
		hdecoder->_buffer.clear();

	}

	cerr << "Requests::testPrintPFile() - SEG7" << endl;

	std::istringstream stream_dna (ddecoder->_buffer);
			
	ddecoder->_buffer.clear();

	std::string line;
	std::string output_buff;

			
	output_buff.reserve(Leon::READ_PER_BLOCK * 500);
			
	bool reading = true;
			
			
	u_int64_t readid=0;
	while(reading){
				
		stringstream sint;
		sint << readid;
				
		

		if( ! noHeader)
		{
			if(getline(*stream_header, line)){
	//			cout << "debug - testPrintReads - getline" << endl;
				if(isFasta)
					output_buff += ">";
				else
					output_buff += "@";
					
				output_buff +=  line + '\n';
			}
			else{
	//			cout << "debug - testPrintReads - getline header false" << endl;
				reading = false;
			}
		}
		else
		{
			if(isFasta)
				output_buff += "> " + sint.str() + '\n';
			else
				output_buff += "@ " + sint.str() + '\n';
					
			readid++;
		}
			 
				
				
		if(getline(stream_dna, line)){
	//		cout << "debug - testPrintReads - getline" << endl;
			output_buff +=  line + '\n';
		}
		else{
	//			cout << "debug - testPrintReads - getline dna false" << endl;
				reading = false;
			}
				
				
		if( ! isFasta)
		{
			cout << " - testPrintPFile - fastq not treated temporarily" << endl;
			//if(getline(*stream_qual, line)){
			//	output_buff += "+\n";
			//	output_buff +=  line + '\n';
			//}
			//else
			//	reading = false;
		}
				 
	}
			
			 
	//_outputFile->fwrite(output_buff.c_str(), output_buff.size(), 1);
	cout << " - testPrintPFile - buff : \n" << output_buff.c_str() << endl;
	//cout << " - testPrintPFile - buff max size : \n" << output_buff.max_size() << endl;
	//cout << " - testPrintPFile - buff actual size : \n" << output_buff.length() << endl;


	if(stream_qual!= NULL) delete  stream_qual;
	if(stream_header!= NULL) delete  stream_header;
			
			
		
		
	//livingThreadCount = 0;

	//cout << "debug testPrintReads : before clearRangeDecoder()" << endl; 
	clearRangeDecoder();
	delete hdecoder;
	delete ddecoder;
	//std::cout << "debug testPrintReads : after clearRangeDecoder()" << endl; 
	//cout << "debug testPrintReads END" << endl; 

}

/*****requests*****/

//TODO find another way to get nb of datasets
/*
void Requests::printNbBanks(){
	std::cout << "number of data sets : " << _nbBanks << std::endl;
}
*/

//unused, to delete
/*
int Requests::getNbBanks(){
	return _nbBanks;
}
*/
void Requests::printKmerSize(){
	std::cout << "kmer size : " << _kmerSize << std::endl << std::endl;
}

int Requests::getKmerSize(){
	return _kmerSize;
}

bool Requests::isKmerInGraph(char* kmer){

	Node node = getKmerNode(kmer);

	return _graph.contains(node);
}

bool Requests::isKmerInGraph(kmer_type kmer){

	Node node = getKmerNode(kmer);

	return _graph.contains(node);
}

bitset<NB_MAX_COLORS> Requests::getKmerColors(char* kmer){

	if (!this->isKmerInGraph(kmer)){
		bitset<8> kmer_colors;
		return kmer_colors;
	}

	Node node = getKmerNode(kmer);

	return _color_array[_graph.nodeMPHFIndex(node)];
}

bitset<NB_MAX_COLORS> Requests::getKmerColors(kmer_type kmer){

	if (!this->isKmerInGraph(kmer)){
		bitset<8> kmer_colors;
		return kmer_colors;
	}

	Node node = getKmerNode(kmer);

	return _color_array[_graph.nodeMPHFIndex(node)];
}

int Requests::getKmerNbColors(char* kmer)
{
	return this->getKmerColors(kmer).count();	
}

bool Requests::isSequenceInGraph(char* sequence){

	int pos = 0;
	char kmer[_kmerSize+1];

	while (this->getSequenceKmer(sequence, pos, kmer)){

		if (!isKmerInGraph(kmer)){
			return false;
		}
		++pos;
	}
	return true;
}
		
bitset<NB_MAX_COLORS> Requests::getSequenceColorsInGraph(char* sequence){

	int pos = 0;
	char kmer[_kmerSize+1];
	bitset<NB_MAX_COLORS> sequence_colors;
	sequence_colors.set();
	
	while (!sequence_colors.none() && this->getSequenceKmer(sequence, pos, kmer)){
		
		if (!isKmerInGraph(kmer)){

			return sequence_colors.reset();
		}

		sequence_colors &= this->getKmerColors(kmer); 

		++pos;
	}
	
	return sequence_colors;

}

int Requests::getSequenceNbColorsInGraph(char* sequence){

	return this->getSequenceColorsInGraph(sequence).count();
}

//TODO stop the algo when constructing the bitset array in getSequenceFileMatchesInData
// if not present to limit timeloop
bool  Requests::isSequenceInData(char* sequence, 
								vector<bitset<NB_MAX_COLORS>>* sequenceMatches/*,
								bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches*/)
{
	return !getSequenceColorsInData(sequence, sequenceMatches/*, _sequenceAmbiguousMatches*/).none();
}

bitset<NB_MAX_COLORS>  Requests::getSequenceColorsInData(char* sequence, 
														vector<bitset<NB_MAX_COLORS>>* sequenceMatches/*,
														bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches*/){

	int sequenceSize = strlen(sequence);
	//bitset<NB_MAX_COLORS> sequenceMatches[sequenceSize];
	getSequenceFileMatchesInData(sequence, sequenceMatches/*, _sequenceAmbiguousMatches*/);

	//tmp debug
	//cerr << "debug Requests::isSequenceInData" << endl;
	//for (int i = 0; i < sequenceSize; ++i)
	//{
	//	cout << (sequenceMatches[i] << " : " << bitset<NB_MAX_COLORS>(sequenceMatches[i]) << endl;
	//}

	int sequencePos = 0;
	//bitset<NB_MAX_COLORS> sequenceColors;
	_sequenceColors.set();
	while (!_sequenceColors.none() && (sequencePos < sequenceSize)){

		_sequenceColors &= (*sequenceMatches)[sequencePos];
		++sequencePos;
	}

	//TODO
	// we keep the 1 values of the bitset _sequenceAmbiguousColors only if
	// the color is fullfilled

	_sequenceAmbiguousColors = _sequenceAmbiguousMatches & _sequenceColors;

	return _sequenceColors;
}

//TODO : count nb colors during construction of bitset sequenceColors
// tolimit time loop
int  Requests::getSequenceNbColorsInData(char* sequence, 
										vector<bitset<NB_MAX_COLORS>>* sequenceMatches/*,
										bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches*/){

	return getSequenceColorsInData(sequence, sequenceMatches/*, _sequenceAmbiguousMatches*/).count();
}

void Requests::getSequenceFileMatchesInData(char* sequence, 
											vector<bitset<NB_MAX_COLORS>>* sequenceMatches/*,
											bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches*/){


	//cerr << "debug Requests::getSequenceFileMatchesInData - BEGIN" << endl;

	//the array with colors of each sequence's part of read
 	int sequenceSize = strlen(sequence);

	int mismatches[NB_MAX_SNPS+1];

	//get existing anchors in the sequence and create a dictionnary of <anchor, pos> 
	u_int64_t dictSize = strlen(sequence);
	u_int64_t nbcreated;
	Hash16<kmer_type, list<u_int32_t>*>* sequenceAnchorKmers = new Hash16<kmer_type, list<u_int32_t>*>(dictSize , &nbcreated);
	fillSequenceAnchorsDict(sequenceAnchorKmers, sequence);
	
	_sequenceAmbiguousMatches.reset();
	
	//decode commpressed file, to find matching anchors
	_filePos = 0;
	initializeRangeDecoder();

	decodeInfos();
	headerSetUp();
	dnaSetUp();
	
	decodeBloom();
	if (! _orderReads){
		decodeAnchorDict();
	}
	else{
		decodeSortedAnchorDict();
	}
	initializeDecoders();

	for (int blockIndice = 0; 
		blockIndice < _dnaBlockSizes.size(); 
		blockIndice += 2){
			

		if(blockIndice >= _dnaBlockSizes.size()) break;
			
		dnaDecoderSetup(blockIndice);
		qualDecoderSetup(blockIndice);

		list<u_int32_t>* listPos;		
		struct ReadInfos* ri = new ReadInfos{};

		if (! _orderReads){
			
			//reading the compressed file read per read
			while(_ddecoder->getNextReadInfos(ri)){

				//if the read's anchor is in the sequence's list of anchors
				//then we try to aline the read to the sequence
				if(sequenceAnchorKmers->get(ri->anchor, &listPos)){

					searchAlignements(sequence, ri, listPos, sequenceAnchorKmers, 
									sequenceMatches/*, _sequenceAmbiguousMatches*/);
				}
			}
		}

		else{
			
			struct OrderedReadsInfosBlock* orib = new OrderedReadsInfosBlock{};
			
			//reading the compressed file read blocks per read blocks
			//a block of read is a block of all the reads with same anchor
			while(_ddecoder->getNextOrderedReadsInfosBLock(orib)){
				
				//if the block's anchor is in the sequence's list of anchors
				//then we try to aline each read of the block to the sequence
				//else, we just read reads and skip without aligning
				if(sequenceAnchorKmers->get(orib->anchor, &listPos) ||
					sequenceAnchorKmers->get(orib->revAnchor, &listPos)){

					for (int i=0; i < orib->nbReads; ++i){
						
						//reading the compressed file block read per read
						if (_ddecoder->getNextOrderedReadInfos(ri)){

							searchAlignements(sequence, ri, listPos, sequenceAnchorKmers, 
								sequenceMatches/*, _sequenceAmbiguousMatches*/);
						}
					}
				}
				else{

					for (int i=0; i < orib->nbReads; ++i){
						_ddecoder->getNextOrderedReadInfos(ri);
					}
				}
			}
		}		
	}

	for (int i = 0; i < sequenceSize; ++i)
	{
		cout << (*sequenceMatches)[i] << " : " << bitset<NB_MAX_COLORS>((*sequenceMatches)[i]) << endl;
	}

	//cleaning decoders objects
	clearRangeDecoder();
	clearDecoders();

	emptySequenceAnchorDict(sequenceAnchorKmers, sequence);
	
}

void Requests::searchAlignements(char* sequence, 
								ReadInfos* ri,
								list<u_int32_t>* listPos,
								Hash16<kmer_type, list<u_int32_t>*>* sequenceAnchorKmers, 
								vector<bitset<NB_MAX_COLORS>>* sequenceMatches/*,
								bitset<NB_MAX_COLORS> _sequenceAmbiguousMatches*/){

	int sequenceSize = strlen(sequence);
	int mismatches[NB_MAX_SNPS+1];
	u_int32_t anchorSequencePos;

	//cerr << "debug Requests::searchAlignements - ri->anchor : " << ri->anchor << endl;

	/*
	//if the read's anchor is in the sequence's list of anchors
	//then we try to aline the read to the sequence
	if(sequenceAnchorKmers->get(ri->anchor, &listPos)){
	*/				


		// iterate on the positions of the list
		// we keep a boolean to know if the read has already been alined
		bool readAlreadyAlined = false;
		for (std::list<u_int32_t>::iterator it=listPos->begin(); it != listPos->end(); ++it){

			anchorSequencePos = *it;
			bitset<NB_MAX_COLORS> readColor = getReadColor(ri);

			char read_chars[ri->sread.size()+1];
			getSequenceChars(read_chars, ri->sread);

			int seqStartPos = max(((int)anchorSequencePos-ri->anchorPos),0);
			int seqPos = seqStartPos;
			int readPos = max((ri->anchorPos-(int)anchorSequencePos),0);
			//memset(mismatches, -1, NB_MAX_SNPS+1);
			for (int i=0; i<NB_MAX_SNPS+1; ++i){
				mismatches[i] = -1;
				//cerr << " mismatches[i] : " << mismatches[i] << endl;
			}
			int nbMismatches = 0;
			//cerr << " mismatches[0] : " << mismatches[0] << endl;

			//compare char per char, 
			while ((seqPos<sequenceSize)&&(readPos<ri->readSize)){

				if(sequence[seqPos] != read_chars[readPos]){
					//cerr << endl << "sequenceSize : " << sequenceSize << endl << "seqPos : " << seqPos << endl << endl;
					//cerr << "readSize : " << ri->readSize << endl << "readPos : " << readPos << endl << endl;
					//cerr << "mismatch at seqPos " << seqPos << ", readPos " << readPos << 
					//" : " << endl << 
					//		 "sequence[seqPos] : " << sequence[seqPos] << endl <<
					//		 "read_chars[readPos] : " << read_chars[readPos] << endl <<
					//		 "read : " << ri->sread << endl;

					++nbMismatches;
					if(nbMismatches > NB_MAX_SNPS){
						//cerr << "nbMismatches > NB_MAX_SNPS" << endl <<endl;
						break;
					}
					//we keep the mismatch position on the sequence
					//for when we'll have to fill the array
					mismatches[nbMismatches-1] = seqPos;
				}

				++seqPos;
				++readPos;
			}
			if(nbMismatches <= NB_MAX_SNPS){
				// cerr << "nbMismatches <= NB_MAX_SNPS" << endl;
				// cerr << "read color : " << readColor << endl;

				if (readAlreadyAlined){
					_sequenceAmbiguousMatches |= readColor;
				}

				int nbMismatchIndex = 0;
				int alignementEndPos = seqPos;
				for (seqPos = seqStartPos; (seqPos < alignementEndPos); ++seqPos)
				{
					// cerr << sequence[seqPos] << " - seqPos : " << seqPos << endl;
					// cerr << " mismatches[nbMismatchIndex] : " << mismatches[nbMismatchIndex] << endl;
					//we verify that it isn't a mismatch position
									
					if (seqPos != mismatches[nbMismatchIndex])
					{
						(*sequenceMatches)[seqPos] |= readColor;
						// cerr << "read color test : " << readColor << endl;
						// cerr << sequence[seqPos] << " - seqPos test : " << seqPos << endl;
					}
					else{
						//we verify that we don't exceed the nbMismatches before increment
						//to avoid seg fault
						if (nbMismatchIndex < nbMismatches-1){
							++nbMismatchIndex;
						}
					}
				}
				readAlreadyAlined = true;
			}
		}				
	/*}*/
}