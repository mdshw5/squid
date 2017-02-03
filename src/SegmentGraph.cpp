#include "SegmentGraph.h"

using namespace std;

void ReverseComplement(string::iterator itbegin, string::iterator itend){
	for(string::iterator it=itbegin; it!=itend; it++)
		*it=Nucleotide[toupper(*it)];
	std::reverse(itbegin, itend);
};

pair<int,int> ExtremeValue(vector<int>::iterator itbegin, vector<int>::iterator itend){
	pair<int,int> x=make_pair(*itbegin, *itbegin);
	for(vector<int>::iterator it=itbegin; it!=itend; it++){
		if((*it)>x.second)
			x.second=(*it);
		if((*it)<x.first)
			x.first=(*it);
	}
	return x;
};

SegmentGraph_t::SegmentGraph_t(const vector<int>& RefLength, SBamrecord_t& SBamrecord, vector< vector<int> >& Read_Node){
	BuildNode(RefLength, SBamrecord);
	BuildEdges(SBamrecord, Read_Node);
	FilterbyWeight();
	FilterbyInterleaving();
	FilterEdges();
	CompressNode(Read_Node);
	FurtherCompressNode();
	ConnectedComponent();
	cout<<vNodes.size()<<'\t'<<vEdges.size()<<endl;
};

SegmentGraph_t::SegmentGraph_t(string graphfile){
	ifstream input(graphfile);
	string line;
	while(getline(input, line)){
		if(line[0]=='#')
			continue;
		vector<string> strs;
		boost::split(strs, line, boost::is_any_of("\t"));
		if(strs[0]=="node"){
			Node_t tmp(stoi(strs[2]), stoi(strs[3]), stoi(strs[4])-stoi(strs[3]), stoi(strs[5]), stod(strs[6]));
			vNodes.push_back(tmp);
		}
		else if(strs[0]=="edge"){
			Edge_t tmp(stoi(strs[2]), (strs[3]=="H"?true:false), stoi(strs[4]), (strs[5]=="H"?true:false), stoi(strs[6]));
			vEdges.push_back(tmp);
		}
	}
	input.close();
	UpdateNodeLink();
	ConnectedComponent();
	cout<<vNodes.size()<<'\t'<<vEdges.size()<<endl;
};

bool SegmentGraph_t::IsDiscordant(int edgeidx){
	int ind1=vEdges[edgeidx].Ind1, ind2=vEdges[edgeidx].Ind2;
	if(vNodes[ind1].Chr!=vNodes[ind2].Chr)
		return true;
	else if(vNodes[ind2].Position-vNodes[ind1].Position-vNodes[ind1].Length>Concord_Dist_Pos && ind2-ind1>Concord_Dist_Idx)
		return true;
	else if(vEdges[edgeidx].Head1!=false || vEdges[edgeidx].Head2!=true)
		return true;
	return false;
};

bool SegmentGraph_t::IsDiscordant(Edge_t* edge){
	int ind1=edge->Ind1, ind2=edge->Ind2;
	if(vNodes[ind1].Chr!=vNodes[ind2].Chr)
		return true;
	else if(vNodes[ind2].Position-vNodes[ind1].Position-vNodes[ind1].Length>Concord_Dist_Pos && ind2-ind1>Concord_Dist_Idx)
		return true;
	else if(edge->Head1!=false || edge->Head2!=true)
		return true;
	return false;
};

void SegmentGraph_t::BuildNode(const vector<int>& RefLength, SBamrecord_t& SBamrecord){
	time_t CurrentTime;
	string CurrentTimeStr;
	vector< pair<int,int> > PartAlignPos;
	PartAlignPos.resize(RefLength.size());
	vector<SingleBamRec_t> bamdiscordant, bamall;
	bamdiscordant.reserve(SBamrecord.size()); bamall.reserve(SBamrecord.size()*3);
	for(vector<ReadRec_t>::const_iterator it=SBamrecord.begin(); it!=SBamrecord.end(); it++){
		for(vector<SingleBamRec_t>::const_iterator itsingle=it->FirstRead.begin(); itsingle!=it->FirstRead.end(); itsingle++)
			bamall.push_back(*itsingle);
		for(vector<SingleBamRec_t>::const_iterator itsingle=it->SecondMate.begin(); itsingle!=it->SecondMate.end(); itsingle++)
			bamall.push_back(*itsingle);
		if(it->IsEndDiscordant(true) || it->IsEndDiscordant(false) || it->IsSingleAnchored() || it->IsPairDiscordant()){
			for(vector<SingleBamRec_t>::const_iterator itsingle=it->FirstRead.begin(); itsingle!=it->FirstRead.end(); itsingle++)
				bamdiscordant.push_back(*itsingle);
			for(vector<SingleBamRec_t>::const_iterator itsingle=it->SecondMate.begin(); itsingle!=it->SecondMate.end(); itsingle++)
				bamdiscordant.push_back(*itsingle);
		}
		else{
			bool firstinserted=false, secondinserted=false;
			int previnserted=-1;
			if(it->FirstRead.size()>0){
				for(int i=0; i<it->FirstRead.size()-1; i++)
					if(abs(it->FirstRead[i].RefPos-it->FirstRead[i+1].RefPos)>750000){
						if(previnserted!=i)
							bamdiscordant.push_back(it->FirstRead[i]);
						bamdiscordant.push_back(it->FirstRead[i+1]);
						previnserted=i+1;
						if(i+1==it->FirstRead.size()-1)
							firstinserted=true;
					}
			}
			previnserted=-1;
			if(it->SecondMate.size()>0){
				for(int i=0; i<it->SecondMate.size()-1; i++)
					if(abs(it->SecondMate[i].RefPos-it->SecondMate[i+1].RefPos)>750000){
						if(previnserted!=i)
							bamdiscordant.push_back(it->SecondMate[i]);
						bamdiscordant.push_back(it->SecondMate[i+1]);
						previnserted=i+1;
						if(i+1==it->SecondMate.size()-1)
							secondinserted=true;
					}
			}
			if(it->FirstRead.size()>0 && it->SecondMate.size()>0){
				if(abs(it->FirstRead.back().RefPos-it->SecondMate.back().RefPos)>750000){
					if(!firstinserted){
						bamdiscordant.push_back(it->FirstRead.back()); firstinserted=true;
					}
					if(!secondinserted){
						bamdiscordant.push_back(it->SecondMate.back()); secondinserted=true;
					}
				}
			}
			if(!firstinserted && !secondinserted){
				if(it->FirstRead.size()!=0 && it->FirstRead.front().ReadPos > 15 && !it->FirstLowPhred)
					PartAlignPos.push_back(make_pair(it->FirstRead[0].RefID, (it->FirstRead[0].IsReverse)? (it->FirstRead[0].RefPos+it->FirstRead[0].MatchRef) : (it->FirstRead[0].RefPos)));
				if(it->FirstRead.size()!=0 && it->FirstTotalLen-it->FirstRead.back().ReadPos-it->FirstRead.back().MatchRead > 15 && !it->FirstLowPhred)
					PartAlignPos.push_back(make_pair(it->FirstRead.back().RefID, (it->FirstRead.back().IsReverse)? (it->FirstRead.back().RefPos) : (it->FirstRead.back().RefPos+it->FirstRead.back().MatchRef)));
				if(it->SecondMate.size()!=0 && it->SecondMate.front().ReadPos > 15 && !it->SecondLowPhred)
					PartAlignPos.push_back(make_pair(it->SecondMate[0].RefID, (it->SecondMate[0].IsReverse)? (it->SecondMate[0].RefPos+it->SecondMate[0].MatchRef) : (it->SecondMate[0].RefPos)));
				if(it->SecondMate.size()!=0 && it->SecondTotalLen-it->SecondMate.back().ReadPos-it->SecondMate.back().MatchRead > 15 && !bamdiscordant.back().Same(it->SecondMate.back()) && !it->SecondLowPhred)
					PartAlignPos.push_back(make_pair(it->SecondMate.back().RefID, (it->SecondMate.back().IsReverse)? (it->SecondMate.back().RefPos) : (it->SecondMate.back().RefPos+it->SecondMate.back().MatchRef)));
			}
		}
	}
	sort(PartAlignPos.begin(), PartAlignPos.end(), [](pair<int,int> a, pair<int,int> b){if(a.first==b.first) return a.second<b.second; else return a.first<b.first;});
	bamdiscordant.reserve(bamdiscordant.size()); bamall.reserve(bamall.size());
	sort(bamdiscordant.begin(), bamdiscordant.end());
	sort(bamall.begin(), bamall.end());
	time(&CurrentTime);
	CurrentTimeStr=ctime(&CurrentTime);
	cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] "<<"Building nodes. |bamall|="<<bamall.size()<<" |bamdiscordant|="<<bamdiscordant.size()<<endl;
	// extend bamdiscordant to build initial nodes
	vector<SingleBamRec_t>::const_iterator itdisstart=bamdiscordant.cbegin();
	vector<SingleBamRec_t>::const_iterator itdisend=bamdiscordant.cbegin();
	vector<SingleBamRec_t>::const_iterator itdiscurrent;
	vector<SingleBamRec_t>::const_iterator itallstart=bamall.cbegin();
	vector<SingleBamRec_t>::const_iterator itallend=bamall.cbegin();
	vector<SingleBamRec_t>::const_iterator itallcurrent;
	vector< pair<int,int> >::iterator itpartstart=PartAlignPos.begin();
	vector< pair<int,int> >::iterator itpartend=PartAlignPos.begin();
	vector< pair<int,int> >::iterator itpartcurrent;
	int thresh=3;
	int prev0CovPos;
	int disrightmost, allrightmost;
	int markedNodeStart=-1, markedNodeChr=-1;
	while(itdisstart!=bamdiscordant.cend()){
		disrightmost=itdisstart->RefPos+itdisstart->MatchRef;
		for(itdisend=itdisstart; itdisend!=bamdiscordant.cend() && itdisend->RefID==itdisstart->RefID && itdisend->RefPos<disrightmost+ReadLen; itdisend++){
			disrightmost = (disrightmost>itdisend->RefPos+itdisend->MatchRef)?disrightmost:(itdisend->RefPos+itdisend->MatchRef);
		}
		for(; itallstart!=bamall.cend() && (itallstart->RefID<itdisstart->RefID || (itallstart->RefID==itdisstart->RefID && vNodes.size()!=0 && vNodes.back().Chr==itdisstart->RefID && itallstart->RefPos+itallstart->MatchRef<vNodes.back().Position+vNodes.back().Length)); itallstart++){}
		prev0CovPos=itallstart->RefPos;
		allrightmost=itallstart->RefPos+itallstart->MatchRef;
		for(; itallstart!=bamall.cend() && itallstart->RefPos+itallstart->MatchRef+ReadLen<itdisstart->RefPos; itallstart++){
			if(itallstart->RefPos>allrightmost)
				prev0CovPos=itallstart->RefPos;
			allrightmost = (allrightmost>itallstart->RefPos+itallstart->MatchRef)?allrightmost:(itallstart->RefPos+itallstart->MatchRef);
		}
		if(itallstart->RefPos>allrightmost)
			prev0CovPos=itallstart->RefPos;
		for(itallend=itallstart; itallend!=bamall.cend() && itallend->RefID==itdisstart->RefID && itallend->RefPos<disrightmost+ReadLen; itallend++){
			allrightmost = (allrightmost>itallend->RefPos+itallend->MatchRef)?allrightmost:(itallend->RefPos+itallend->MatchRef);
		}
		for(; itpartstart!=PartAlignPos.end() && (itpartstart->first<itdisstart->RefID || (itpartstart->first==itdisstart->RefID && itpartstart->second+ReadLen<itdisstart->RefPos)); itpartstart++){}
		for(itpartend=itpartstart; itpartend!=PartAlignPos.end() && itpartend->first==itdisstart->RefID && itpartend->second<disrightmost+ReadLen; itpartend++){}

		int curEndPos=0, curStartPos=(prev0CovPos>markedNodeStart)?prev0CovPos:markedNodeStart;
		int disStartPos=-1, disEndPos=-1, disCount=-1;
		bool isClusternSplit=false;
		while(itdisstart!=itdisend){
			if(disStartPos!=-1 && !isClusternSplit && disCount>min(5.0, 4.0*(disEndPos-disStartPos)/ReadLen)){
				if(vNodes.size()!=0 && vNodes.back().Chr==itdisstart->RefID && disEndPos-vNodes.back().Position-vNodes.back().Length<thresh*20)
					vNodes.back().Length+=disEndPos-vNodes.back().Position-vNodes.back().Length;
				else{
					Node_t tmp(itdisstart->RefID, disStartPos, disEndPos-disStartPos);
					vNodes.push_back(tmp);
				}
				curStartPos=disEndPos; curEndPos=disEndPos;
				markedNodeStart=disEndPos; markedNodeChr=itdisstart->RefID;
			}
			isClusternSplit=false;
			vector<int> MarginPositions;
			for(itdiscurrent=itdisstart; itdiscurrent!=itdisend; itdiscurrent++){
				MarginPositions.push_back(itdiscurrent->RefPos); MarginPositions.push_back(itdiscurrent->RefPos+itdiscurrent->MatchRef);
				curEndPos=(curEndPos>MarginPositions.back())?curEndPos:MarginPositions.back();
				if((itdiscurrent+1)!=itdisend){
					if((itdiscurrent+1)->RefPos>itdiscurrent->RefPos+itdiscurrent->MatchRef)
						break;
				}
			}
			disStartPos=max(curStartPos, itdisstart->RefPos);
			disEndPos=curEndPos;
			disCount=distance(itdisstart, itdiscurrent);
			if(itdiscurrent!=itdisend){
				for(itdiscurrent++; itdiscurrent!=itdisend && itdiscurrent->RefPos<curEndPos+thresh; itdiscurrent++){
					MarginPositions.push_back(itdiscurrent->RefPos); MarginPositions.push_back(itdiscurrent->RefPos+itdiscurrent->MatchRef);
				}
			}
			for(itpartcurrent=itpartstart; itpartcurrent!=itpartend && itpartcurrent->second<curEndPos+thresh; itpartcurrent++){
				MarginPositions.push_back(itpartcurrent->second);
			}
			sort(MarginPositions.begin(), MarginPositions.end());
			int lastCurser=-1, lastSupport=0;
			for(vector<int>::iterator itbreak=MarginPositions.begin(); itbreak!=MarginPositions.end(); itbreak++){
				if(vNodes.size()!=0 && vNodes.back().Chr==itdisstart->RefID && (*itbreak)-vNodes.back().Position-vNodes.back().Length<thresh*20)
					continue;
				vector<int>::iterator itbreaknext=itbreak, itbreak2;
				int srsupport=0, peleftfor=0, perightrev=0;
				for(itbreak2=MarginPositions.begin(); itbreak2!=MarginPositions.end() && (*itbreak2)<(*itbreak)+thresh; itbreak2++){
					if(abs((*itbreak)-(*itbreak2))<thresh)
						srsupport++;
				}
				for(itdiscurrent=itdisstart; itdiscurrent!=itdisend; itdiscurrent++){
					if(itdiscurrent->RefPos+itdiscurrent->MatchRef<(*itbreak) && itdiscurrent->RefPos+itdiscurrent->MatchRef>(*itbreak)-ReadLen && !itdiscurrent->IsReverse)
						peleftfor++;
					else if(itdiscurrent->RefPos>(*itbreak) && itdiscurrent->RefPos<(*itbreak)+ReadLen && itdiscurrent->IsReverse)
						perightrev++;
				}
				if(srsupport>3 || srsupport+peleftfor>4 || srsupport+perightrev>4){ // it is a cluster, compare with coverage to decide whether node ends here
					int coverage=0;
					for(itallcurrent=itallstart; itallcurrent!=itallend; itallcurrent++){
						if(itallcurrent->RefPos+itallcurrent->MatchRef>=(*itbreak)+thresh && itallcurrent->RefPos<(*itbreak)-thresh) // aligner are trying to extend match as much as possible, so use thresh
							coverage++;
					}
					if(srsupport>max(coverage-srsupport, 0)+2){
						if(lastCurser==-1 && (*itbreak)-curStartPos<thresh*20){
							markedNodeStart=curStartPos; markedNodeChr=itdisstart->RefID;
						}
						else if((lastCurser==-1 || (*itbreak)-lastCurser<thresh*20) && max(srsupport+peleftfor, srsupport+perightrev)>lastSupport){ // if this breakpoint is near enough to the last, only keep 1, this or last based on support
							lastCurser=(*itbreak); lastSupport=max(srsupport+peleftfor, srsupport+perightrev);
						}
						else if((*itbreak)-lastCurser>=thresh*20){
							isClusternSplit=true;
							Node_t tmp(itdisstart->RefID, curStartPos, lastCurser-curStartPos);
							vNodes.push_back(tmp);
							curStartPos=lastCurser; curEndPos=lastCurser;
							markedNodeStart=lastCurser; markedNodeChr=tmp.Chr;
							break;
						}
					}
				}
				for(itbreaknext=itbreak; itbreaknext!=MarginPositions.end() && (*itbreaknext)==(*itbreak); itbreaknext++){}
				if(itbreaknext!=MarginPositions.end()){
					itbreak=itbreaknext; itbreak--;
				}
				else
					break;
			}
			if(lastCurser!=-1 && !isClusternSplit){
				isClusternSplit=true;
				Node_t tmp(itdisstart->RefID, curStartPos, lastCurser-curStartPos);
				vNodes.push_back(tmp);
				curStartPos=lastCurser; curEndPos=lastCurser;
				markedNodeStart=lastCurser; markedNodeChr=tmp.Chr;
			}
			while(itdisstart!=itdisend && itdisstart->RefPos+itdisstart->MatchRef<=curEndPos)
				itdisstart++;
		}
		if(disStartPos!=-1 && !isClusternSplit && disCount>min(5.0, 4.0*(disEndPos-disStartPos)/ReadLen)){
			if(vNodes.size()!=0 && vNodes.back().Chr==(itdisend-1)->RefID && disEndPos-vNodes.back().Position-vNodes.back().Length<thresh*20)
				vNodes.back().Length+=disEndPos-vNodes.back().Position-vNodes.back().Length;
			else{
				Node_t tmp((itdisend-1)->RefID, disStartPos, disEndPos-disStartPos);
				vNodes.push_back(tmp);
			}
			curStartPos=disEndPos; curEndPos=disEndPos;
			markedNodeStart=disEndPos; markedNodeChr=(itdisend-1)->RefID;
		}

		for(itallend=itallstart; itallend!=bamall.cend() && itallend->RefID==(itdisend-1)->RefID; itallend++){
			if((itdisend-1)->RefID==itdisend->RefID && itallend->RefPos+itallend->MatchRef+ReadLen>=itdisend->RefPos)
				break;
			else if(itallend->RefPos>allrightmost){
				if(markedNodeStart!=-1){
					if(allrightmost>markedNodeStart && allrightmost-markedNodeStart<thresh*20 && vNodes.size()>0 && markedNodeStart==vNodes.back().Position+vNodes.back().Length){
						vNodes.back().Length+=allrightmost-markedNodeStart;
					}
					else if(allrightmost>markedNodeStart && allrightmost-markedNodeStart>=thresh*20){
						Node_t tmp(markedNodeChr, markedNodeStart, allrightmost-markedNodeStart);
						vNodes.push_back(tmp);
					}
					markedNodeStart=-1; markedNodeChr=-1;
					break;
				}
			}
			allrightmost = (allrightmost>itallend->RefPos+itallend->MatchRef)?allrightmost:(itallend->RefPos+itallend->MatchRef);
		}
		if(itallend->RefID!=(itdisend-1)->RefID && markedNodeStart!=-1){
			if(allrightmost>markedNodeStart && allrightmost-markedNodeStart<thresh*20 && vNodes.size()>0 && markedNodeStart==vNodes.back().Position+vNodes.back().Length){
				vNodes.back().Length+=allrightmost-markedNodeStart;
			}
			else if(allrightmost>markedNodeStart && allrightmost-markedNodeStart>=thresh*20){
				Node_t tmp(markedNodeChr, markedNodeStart, allrightmost-markedNodeStart);
				vNodes.push_back(tmp);
			}
			markedNodeStart=-1; markedNodeChr=-1;
		}

		itdisstart=itdisend;
	}
	time(&CurrentTime);
	CurrentTimeStr=ctime(&CurrentTime);
	cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] Building nodes, finish seeding."<<endl;
	// checking node
	for(int i=0; i<vNodes.size(); i++){
		assert(vNodes[i].Length>0 && vNodes[i].Position+vNodes[i].Length<=RefLength[vNodes[i].Chr]);
		if(i+1<vNodes.size())
			assert((vNodes[i].Chr!=vNodes[i+1].Chr) || (vNodes[i].Chr==vNodes[i+1].Chr && vNodes[i].Position+vNodes[i].Length<=vNodes[i+1].Position));
	}
	// expand nodes to cover whole genome
	vector<Node_t> tmpNodes;
	for(int i=0; i<vNodes.size(); i++){
		if(tmpNodes.size()==0 || tmpNodes.back().Chr!=vNodes[i].Chr){
			if(tmpNodes.size()!=0 && tmpNodes.back().Position+tmpNodes.back().Length!=RefLength[tmpNodes.back().Chr]){
				Node_t tmp(tmpNodes.back().Chr, tmpNodes.back().Position+tmpNodes.back().Length, RefLength[tmpNodes.back().Chr]-tmpNodes.back().Position-tmpNodes.back().Length);
				tmpNodes.push_back(tmp);
			}
			int chrstart=(tmpNodes.size()==0)?0:(tmpNodes.back().Chr+1);
			for(; chrstart!=vNodes[i].Chr; chrstart++){
				Node_t tmp(chrstart, 0, RefLength[chrstart]);
				tmpNodes.push_back(tmp);
			}
			if(vNodes[i].Position!=0){
				if(vNodes[i].Position>100){
					Node_t tmp(vNodes[i].Chr, 0, vNodes[i].Position);
					tmpNodes.push_back(tmp);
				}
				else{
					vNodes[i].Length+=vNodes[i].Position; vNodes[i].Position=0;
					tmpNodes.push_back(vNodes[i]);
					continue;
				}
			}
		}
		if(tmpNodes.back().Position+tmpNodes.back().Length<vNodes[i].Position){
			if(vNodes[i].Position-tmpNodes.back().Position-tmpNodes.back().Length>100){
				Node_t tmp(vNodes[i].Chr, tmpNodes.back().Position+tmpNodes.back().Length, vNodes[i].Position-tmpNodes.back().Position-tmpNodes.back().Length);
				tmpNodes.push_back(tmp);
				tmpNodes.push_back(vNodes[i]);
			}
			else{
				vNodes[i].Length+=vNodes[i].Position-tmpNodes.back().Position-tmpNodes.back().Length;
				vNodes[i].Position=tmpNodes.back().Position+tmpNodes.back().Length;
				tmpNodes.push_back(vNodes[i]);
			}
		}
		else
			tmpNodes.push_back(vNodes[i]);
	}
	if(tmpNodes.size()!=0 && tmpNodes.back().Position+tmpNodes.back().Length!=RefLength[tmpNodes.back().Chr]){
		Node_t tmp(tmpNodes.back().Chr, tmpNodes.back().Position+tmpNodes.back().Length, RefLength[tmpNodes.back().Chr]-tmpNodes.back().Position-tmpNodes.back().Length);
		tmpNodes.push_back(tmp);
	}
	for(int chrstart=tmpNodes.back().Chr+1; chrstart<RefLength.size(); chrstart++){
		Node_t tmp(chrstart, 0, RefLength[chrstart]);
		tmpNodes.push_back(tmp);
	}
	vNodes=tmpNodes; tmpNodes.clear();
	time(&CurrentTime);
	CurrentTimeStr=ctime(&CurrentTime);
	cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] Building nodes, finish expanding to whole genome."<<endl;
	// calculate read count for each node
	vector<SingleBamRec_t>::const_iterator itall=bamall.cbegin();
	for(int i=0; i<vNodes.size(); i++){
		if(i%1000000==0){
			time(&CurrentTime);
			CurrentTimeStr=ctime(&CurrentTime);
			cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] Building nodes, calculating read coverage for node "<<i<<"."<<endl;
		}
		int count=0, sumlen=0;
		for(; itall!=bamall.end() && itall->RefID==vNodes[i].Chr && itall->RefPos<vNodes[i].Position+vNodes[i].Length; itall++)
			if(itall->RefPos>=vNodes[i].Position && itall->RefPos+itall->MatchRef<=vNodes[i].Position+vNodes[i].Length){
				count++; sumlen+=itall->MatchRef;
			}
		vNodes[i].Support=count;
		vNodes[i].AvgDepth=1.0*sumlen/vNodes[i].Length;
	}
	time(&CurrentTime);
	CurrentTimeStr=ctime(&CurrentTime);
	cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] Finish calculating reads per node."<<endl;
};

vector<int> SegmentGraph_t::LocateRead(int initialguess, ReadRec_t& ReadRec){
	vector<int> tmpRead_Node((int)ReadRec.FirstRead.size()+(int)ReadRec.SecondMate.size(), 0);
	int i=initialguess, thresh=5; // maximum overhanging length if read is not totally within 1 node
	for(int k=0; k<ReadRec.FirstRead.size(); k++){
		if(i<0 || i>=vNodes.size())
			i=initialguess;
		if(!(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)){
			if(vNodes[i].Chr<ReadRec.FirstRead[k].RefID || (vNodes[i].Chr==ReadRec.FirstRead[k].RefID && vNodes[i].Position<=ReadRec.FirstRead[k].RefPos)){
				for(; i<vNodes.size() && vNodes[i].Chr<=ReadRec.FirstRead[k].RefID; i++)
					if(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
						break;
			}
			else{
				for(; i>-1 && vNodes[i].Chr>=ReadRec.FirstRead[k].RefID; i--)
					if(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
						break;
			}
		}
		if(i<0 || i>=vNodes.size() || vNodes[i].Chr!=ReadRec.FirstRead[k].RefID)
			tmpRead_Node[k]=-1;
		else{
			tmpRead_Node[k]=i;
			if(ReadRec.FirstRead[k].RefPos<vNodes[i].Position){
				int oldRefPos=ReadRec.FirstRead[k].RefPos;
				if(!ReadRec.FirstRead[k].IsReverse){
					ReadRec.FirstRead[k].ReadPos+=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRead-=(vNodes[i].Position-oldRefPos);
					ReadRec.FirstRead[k].RefPos=vNodes[i].Position;
				}
				else{
					ReadRec.FirstRead[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRead-=(vNodes[i].Position-oldRefPos);
					ReadRec.FirstRead[k].RefPos=vNodes[i].Position;
				}
			}
			if(ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef>vNodes[i].Position+vNodes[i].Length){
				int oldEndPos=ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef;
				if(!ReadRec.FirstRead[k].IsReverse){
					ReadRec.FirstRead[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
				}
				else{
					ReadRec.FirstRead[k].ReadPos+=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
				}
			}
		}
	}
	for(int k=0; k<ReadRec.SecondMate.size(); k++){
		if(i<0 || i>=vNodes.size())
			i=initialguess;
		if(!(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)){
			if(vNodes[i].Chr<ReadRec.SecondMate[k].RefID || (vNodes[i].Chr==ReadRec.SecondMate[k].RefID && vNodes[i].Position<=ReadRec.SecondMate[k].RefPos)){
				for(; i<vNodes.size() && vNodes[i].Chr<=ReadRec.SecondMate[k].RefID; i++)
					if(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
						break;
			}
			else{
				for(; i>-1 && vNodes[i].Chr>=ReadRec.SecondMate[k].RefID; i--)
					if(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
						break;
			}
		}
		if(i<0 || i>=vNodes.size() || vNodes[i].Chr!=ReadRec.SecondMate[k].RefID)
			tmpRead_Node[(int)ReadRec.FirstRead.size()+k]=-1;
		else{
			tmpRead_Node[(int)ReadRec.FirstRead.size()+k]=i;
			if(ReadRec.SecondMate[k].RefPos<vNodes[i].Position){
				int oldRefPos=ReadRec.SecondMate[k].RefPos;
				if(!ReadRec.SecondMate[k].IsReverse){
					ReadRec.SecondMate[k].ReadPos+=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRead-=(vNodes[i].Position-oldRefPos);
					ReadRec.SecondMate[k].RefPos=vNodes[i].Position;
				}
				else{
					ReadRec.SecondMate[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRead-=(vNodes[i].Position-oldRefPos);
					ReadRec.SecondMate[k].RefPos=vNodes[i].Position;
				}
			}
			if(ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef>vNodes[i].Position+vNodes[i].Length){
				int oldEndPos=ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef;
				if(!ReadRec.SecondMate[k].IsReverse){
					ReadRec.SecondMate[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
				}
				else{
					ReadRec.SecondMate[k].ReadPos+=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
				}
			}
		}
	}
	return tmpRead_Node;
};

vector<int> SegmentGraph_t::LocateRead(vector<int>& singleRead_Node, ReadRec_t& ReadRec){
	int initialguess=0, thresh=5;
	for(int i=0; i<singleRead_Node.size(); i++)
		if(singleRead_Node[i]!=-1){
			initialguess=singleRead_Node[i]; break;
		}
	if(singleRead_Node.size()!=ReadRec.FirstRead.size()+ReadRec.SecondMate.size()){
		vector<int> tmpRead_Node=LocateRead(initialguess, ReadRec);
		return tmpRead_Node;
	}
	else{
		vector<int> tmpRead_Node((int)ReadRec.FirstRead.size()+(int)ReadRec.SecondMate.size(), 0);
		int i=0;
		for(int k=0; k<ReadRec.FirstRead.size(); k++){
			i=(singleRead_Node[k]==-1)?i:singleRead_Node[k];
			i=(i==-1)?initialguess:i;
			if(!(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)){
				if(vNodes[i].Chr<ReadRec.FirstRead[k].RefID || (vNodes[i].Chr==ReadRec.FirstRead[k].RefID && vNodes[i].Position<=ReadRec.FirstRead[k].RefPos)){
					for(; i<vNodes.size() && vNodes[i].Chr<=ReadRec.FirstRead[k].RefID; i++)
						if(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
							break;
				}
				else{
					for(; i>=0 && vNodes[i].Chr>=ReadRec.FirstRead[k].RefID; i--)
						if(vNodes[i].Chr==ReadRec.FirstRead[k].RefID && ReadRec.FirstRead[k].RefPos>=vNodes[i].Position-thresh && ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
							break;
				}
			}
			if(i<0 || i>=vNodes.size() || vNodes[i].Chr!=ReadRec.FirstRead[k].RefID)
				tmpRead_Node[k]=-1;
			else{
				tmpRead_Node[k]=i;
				if(ReadRec.FirstRead[k].RefPos<vNodes[i].Position){
					int oldRefPos=ReadRec.FirstRead[k].RefPos;
					if(!ReadRec.FirstRead[k].IsReverse){
						ReadRec.FirstRead[k].ReadPos+=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRead-=(vNodes[i].Position-oldRefPos);
						ReadRec.FirstRead[k].RefPos=vNodes[i].Position;
					}
					else{
						ReadRec.FirstRead[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.FirstRead[k].MatchRead-=(vNodes[i].Position-oldRefPos);
						ReadRec.FirstRead[k].RefPos=vNodes[i].Position;
					}
				}
				if(ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef>vNodes[i].Position+vNodes[i].Length){
					int oldEndPos=ReadRec.FirstRead[k].RefPos+ReadRec.FirstRead[k].MatchRef;
					if(!ReadRec.FirstRead[k].IsReverse){
						ReadRec.FirstRead[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
					}
					else{
						ReadRec.FirstRead[k].ReadPos+=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.FirstRead[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
					}
				}
			}
		}
		for(int k=0; k<ReadRec.SecondMate.size(); k++){
			i=(singleRead_Node[(int)ReadRec.FirstRead.size()+k]==-1)?i:singleRead_Node[(int)ReadRec.FirstRead.size()+k];
			i=(i==-1)?initialguess:i;
			if(!(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)){
				if(vNodes[i].Chr<ReadRec.SecondMate[k].RefID || (vNodes[i].Chr==ReadRec.SecondMate[k].RefID && vNodes[i].Position<=ReadRec.SecondMate[k].RefPos)){
					for(; i<vNodes.size() && vNodes[i].Chr<=ReadRec.SecondMate[k].RefID; i++)
						if(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
							break;
				}
				else{
					for(; i>=0 && vNodes[i].Chr>=ReadRec.SecondMate[k].RefID; i--)
						if(vNodes[i].Chr==ReadRec.SecondMate[k].RefID && ReadRec.SecondMate[k].RefPos>=vNodes[i].Position-thresh && ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef<=vNodes[i].Position+vNodes[i].Length+thresh)
							break;
				}
			}
			if(i<0 || i>=vNodes.size() || vNodes[i].Chr!=ReadRec.SecondMate[k].RefID)
				tmpRead_Node[(int)ReadRec.FirstRead.size()+k]=-1;
			else{
				tmpRead_Node[(int)ReadRec.FirstRead.size()+k]=i;
				if(ReadRec.SecondMate[k].RefPos<vNodes[i].Position){
					int oldRefPos=ReadRec.SecondMate[k].RefPos;
					if(!ReadRec.SecondMate[k].IsReverse){
						ReadRec.SecondMate[k].ReadPos+=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRead-=(vNodes[i].Position-oldRefPos);
						ReadRec.SecondMate[k].RefPos=vNodes[i].Position;
					}
					else{
						ReadRec.SecondMate[k].MatchRef-=(vNodes[i].Position-oldRefPos); ReadRec.SecondMate[k].MatchRead-=(vNodes[i].Position-oldRefPos);
						ReadRec.SecondMate[k].RefPos=vNodes[i].Position;
					}
				}
				if(ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef>vNodes[i].Position+vNodes[i].Length){
					int oldEndPos=ReadRec.SecondMate[k].RefPos+ReadRec.SecondMate[k].MatchRef;
					if(!ReadRec.SecondMate[k].IsReverse){
						ReadRec.SecondMate[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
					}
					else{
						ReadRec.SecondMate[k].ReadPos+=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRef-=(oldEndPos-vNodes[i].Position-vNodes[i].Length); ReadRec.SecondMate[k].MatchRead-=(oldEndPos-vNodes[i].Position-vNodes[i].Length);
					}
				}
			}
		}
		return tmpRead_Node;
	}
};

void SegmentGraph_t::RawEdges(SBamrecord_t& SBamrecord, vector< vector<int> >& Read_Node){
	int firstfrontindex=0, i=0, j=0, splittedcount=0;
	clock_t starttime=clock();
	for(vector<ReadRec_t>::iterator it=SBamrecord.begin(); it!=SBamrecord.end(); it++){
		if(it->FirstRead.size()==0 && it->SecondMate.size()==0)
			continue;
		vector<int> tmpRead_Node=LocateRead(firstfrontindex, *it);
		if(tmpRead_Node[0]!=-1)
			firstfrontindex=tmpRead_Node[0];
		for(int k=0; k<tmpRead_Node.size(); k++)
			if(tmpRead_Node[k]==-1){
				i=firstfrontindex; splittedcount++;
				if(k<(int)it->FirstRead.size()){
					for(; i<vNodes.size() && (vNodes[i].Chr<it->FirstRead[k].RefID || (vNodes[i].Chr==it->FirstRead[k].RefID && vNodes[i].Position+vNodes[i].Length<it->FirstRead[k].RefPos)); i++){}
					for(; i>-1 && (vNodes[i].Chr>it->FirstRead[k].RefID || (vNodes[i].Chr==it->FirstRead[k].RefID && vNodes[i].Position>it->FirstRead[k].RefPos)); i--){}
					Edge_t tmp(i, false, i+1, true);
					vEdges.push_back(tmp);
				}
				else if(k<(int)it->FirstRead.size()+(int)it->SecondMate.size()){
					k-=(int)it->FirstRead.size();
					for(; i<vNodes.size() && (vNodes[i].Chr<it->SecondMate[k].RefID || (vNodes[i].Chr==it->SecondMate[k].RefID && vNodes[i].Position+vNodes[i].Length<it->SecondMate[k].RefPos)); i++){}
					for(; i>-1 && (vNodes[i].Chr>it->SecondMate[k].RefID || (vNodes[i].Chr==it->SecondMate[k].RefID && vNodes[i].Position>it->SecondMate[k].RefPos)); i--){}
					Edge_t tmp(i, false, i+1, true);
					vEdges.push_back(tmp);
					k+=(int)it->FirstRead.size();
				}
			}
		if(distance(SBamrecord.begin(), it)%1000000==0)
			cout<<distance(SBamrecord.begin(), it)<<"\ttime="<<(1.0*(clock()-starttime)/CLOCKS_PER_SEC)<<endl;
		// edges from FirstRead segments
		if(it->FirstRead.size()>0){
			for(int k=0; k<it->FirstRead.size()-1; k++){
				i=tmpRead_Node[k]; j=tmpRead_Node[k+1];
				if(i!=j && i!=-1 && j!=-1){
					bool tmpHead1=(it->FirstRead[k].IsReverse)?true:false, tmpHead2=(it->FirstRead[k+1].IsReverse)?false:true;
					Edge_t tmp(i, tmpHead1, j, tmpHead2, 1);
					assert(tmp.Ind1>=0 && tmp.Ind1<(int)vNodes.size() && tmp.Ind2>=0 && tmp.Ind2<(int)vNodes.size());
					vEdges.push_back(tmp);
				}
			}
		}
		// edges from SecondMate segments
		if(it->SecondMate.size()>0){
			for(int k=0; k<it->SecondMate.size()-1; k++){
				i=tmpRead_Node[(int)it->FirstRead.size()+k]; j=tmpRead_Node[(int)it->FirstRead.size()+k+1];
				if(i!=j && i!=-1 && j!=-1){
					bool tmpHead1=(it->SecondMate[k].IsReverse)?true:false, tmpHead2=(it->SecondMate[k+1].IsReverse)?false:true;
					Edge_t tmp(i, tmpHead1, j, tmpHead2, 1);
					assert(tmp.Ind1>=0 && tmp.Ind1<(int)vNodes.size() && tmp.Ind2>=0 && tmp.Ind2<(int)vNodes.size());
					vEdges.push_back(tmp);
				}
			}
		}
		// edges from pair ends
		if(it->FirstRead.size()>0 && it->SecondMate.size()>0){
			if(!it->IsSingleAnchored() && !it->IsEndDiscordant(true) && !it->IsEndDiscordant(false)){
				i=tmpRead_Node[(int)it->FirstRead.size()-1]; j=tmpRead_Node.back();
				bool isoverlap=false;
				for(int k=0; k<it->FirstRead.size(); k++)
					if(j==tmpRead_Node[k])
						isoverlap=true;
				for(int k=0; k<it->SecondMate.size(); k++)
					if(i==tmpRead_Node[(int)it->FirstRead.size()+k])
						isoverlap=true;
				if(i!=j && i!=-1 && j!=-1 && !isoverlap){
					bool tmpHead1=(it->FirstRead.back().IsReverse)?true:false, tmpHead2=(it->SecondMate.back().IsReverse)?true:false;
					Edge_t tmp(i, tmpHead1, j, tmpHead2, 1);
					assert(tmp.Ind1>=0 && tmp.Ind1<(int)vNodes.size() && tmp.Ind2>=0 && tmp.Ind2<(int)vNodes.size());
					vEdges.push_back(tmp);
				}
			}
		}
		Read_Node.push_back(tmpRead_Node);
	}
	cout<<"number splitted reads = "<<splittedcount<<endl;
};

void SegmentGraph_t::BuildEdges(SBamrecord_t& SBamrecord, vector< vector<int> >& Read_Node){
	time_t CurrentTime;
	string CurrentTimeStr;

	vector<Edge_t> tmpEdges; tmpEdges.reserve(vEdges.size());
	RawEdges(SBamrecord, Read_Node);
	sort(vEdges.begin(), vEdges.end());
	for(int i=0; i<vEdges.size(); i++){
		if(tmpEdges.size()==0 || !(vEdges[i]==tmpEdges.back()))
			tmpEdges.push_back(vEdges[i]);
		else
			tmpEdges.back().Weight++;
	}
	tmpEdges.reserve(tmpEdges.size());
	vEdges=tmpEdges;
	UpdateNodeLink();
	tmpEdges.clear();

	time(&CurrentTime);
	CurrentTimeStr=ctime(&CurrentTime);
	cout<<"["<<CurrentTimeStr.substr(0, CurrentTimeStr.size()-1)<<"] Finish building edges."<<endl;
};

void SegmentGraph_t::FilterbyWeight(){
	int relaxedweight=Min_Edge_Weight-2; // use relaxed weight first, to filter more multi-linked bad nodes
	vector<Edge_t> tmpEdges; tmpEdges.reserve(vEdges.size());
	for(int i=0; i<vEdges.size(); i++){
		int sumnearweight=0;
		int chr1=vNodes[vEdges[i].Ind1].Chr, pos1=(vEdges[i].Head1)?vNodes[vEdges[i].Ind1].Position:(vNodes[vEdges[i].Ind1].Position+vNodes[vEdges[i].Ind1].Length);
		int chr2=vNodes[vEdges[i].Ind2].Chr, pos2=(vEdges[i].Head2)?vNodes[vEdges[i].Ind2].Position:(vNodes[vEdges[i].Ind2].Position+vNodes[vEdges[i].Ind2].Length);
		vector<Edge_t> nearEdges;
		nearEdges.push_back(vEdges[i]);
		for(int j=i-1; j>-1 && vEdges[j].Ind1>=vEdges[i].Ind1-Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length>=pos1-Concord_Dist_Pos; j--){
			int newchr1=vNodes[vEdges[j].Ind1].Chr, newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
			int newchr2=vNodes[vEdges[j].Ind2].Chr, newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
			if(vEdges[j].Ind2>vEdges[i].Ind1 && vEdges[i].Head1==vEdges[j].Head1 && vEdges[i].Head2==vEdges[j].Head2 && newchr1==chr1 && newchr2==chr2 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos)
				nearEdges.push_back(vEdges[j]);
		}
		for(int j=i+1; j<vEdges.size() && vEdges[j].Ind1<=vEdges[i].Ind1+Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position<=pos1+Concord_Dist_Pos; j++){
			int newchr1=vNodes[vEdges[j].Ind1].Chr, newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
			int newchr2=vNodes[vEdges[j].Ind2].Chr, newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
			if(vEdges[j].Ind1<vEdges[i].Ind2 && vEdges[i].Head1==vEdges[j].Head1 && vEdges[i].Head2==vEdges[j].Head2 && newchr1==chr1 && newchr2==chr2 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos)
				nearEdges.push_back(vEdges[j]);
		}
		sort(nearEdges.begin(), nearEdges.end());
		for(vector<Edge_t>::iterator itedge=nearEdges.begin(); itedge!=nearEdges.end(); itedge++)
			sumnearweight+=itedge->Weight;
		if(sumnearweight>relaxedweight)
			for(int j=0; j<nearEdges.size(); j++){
				nearEdges[j].GroupWeight=sumnearweight; tmpEdges.push_back(nearEdges[j]);
			}
	}
	sort(tmpEdges.begin(), tmpEdges.end());
	vector<Edge_t>::iterator endit=unique(tmpEdges.begin(), tmpEdges.end());
	tmpEdges.resize(distance(tmpEdges.begin(), endit));
	vEdges=tmpEdges;
	UpdateNodeLink();
};

void SegmentGraph_t::FilterbyInterleaving(){
	// two types of impossible patterns: interleaving nodes; a node with head edges and tail edges overlapping a lot
	vector<Edge_t> ImpossibleEdges; ImpossibleEdges.reserve(vEdges.size());
	vector<Edge_t> tmpEdges; tmpEdges.resize(vEdges.size());
	for(int i=0; i<vEdges.size(); i++){
		if(vEdges[i].Ind2-vEdges[i].Ind1<=Concord_Dist_Idx ||  (vNodes[vEdges[i].Ind1].Chr==vNodes[vEdges[i].Ind2].Chr && abs(vNodes[vEdges[i].Ind1].Position-vNodes[vEdges[i].Ind2].Position)<=Concord_Dist_Pos))
			continue;
		bool whetherdelete=false;
		vector<Edge_t> PotentialEdges; PotentialEdges.push_back(vEdges[i]);
		vector<int> Anch1Head, Anch2Head;
		vector<int> Anch1Tail, Anch2Tail;
		if(vEdges[i].Head1)
			Anch1Head.push_back(vEdges[i].Ind2);
		else
			Anch1Tail.push_back(vEdges[i].Ind2);
		if(vEdges[i].Head2)
			Anch2Head.push_back(vEdges[i].Ind1);
		else
			Anch2Tail.push_back(vEdges[i].Ind1);
		int chr1=vNodes[vEdges[i].Ind1].Chr, pos1=(vEdges[i].Head1)?vNodes[vEdges[i].Ind1].Position:(vNodes[vEdges[i].Ind1].Position+vNodes[vEdges[i].Ind1].Length);
		int chr2=vNodes[vEdges[i].Ind2].Chr, pos2=(vEdges[i].Head2)?vNodes[vEdges[i].Ind2].Position:(vNodes[vEdges[i].Ind2].Position+vNodes[vEdges[i].Ind2].Length);
		for(int j=i-1; j>-1 && vEdges[j].Ind1>=vEdges[i].Ind1-Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length>=pos1-Concord_Dist_Pos; j--){
			int newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
			int newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
			if(vEdges[j].Ind2>vEdges[i].Ind1 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos){
				PotentialEdges.push_back(vEdges[j]);
				if(vEdges[j].Ind1<=vEdges[i].Ind1 && vEdges[j].Head1)
					Anch1Head.push_back(vEdges[j].Ind2);
				else if(vEdges[j].Ind1>=vEdges[i].Ind1 && !vEdges[j].Head1)
					Anch1Tail.push_back(vEdges[j].Ind2);
				if(vEdges[j].Ind2<=vEdges[i].Ind2 && vEdges[j].Head2)
					Anch2Head.push_back(vEdges[j].Ind1);
				else if(vEdges[j].Ind2>=vEdges[i].Ind2 && !vEdges[j].Head2)
					Anch2Tail.push_back(vEdges[j].Ind1);
			}
		}
		for(int j=i+1; j<vEdges.size() && vEdges[j].Ind1<=vEdges[i].Ind1+Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position<=pos1+Concord_Dist_Pos; j++){
			int newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
			int newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
			if(vEdges[j].Ind1<vEdges[i].Ind2 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos){
				PotentialEdges.push_back(vEdges[j]);
				if(vEdges[j].Ind1<=vEdges[i].Ind1 && vEdges[j].Head1)
					Anch1Head.push_back(vEdges[j].Ind2);
				else if(vEdges[j].Ind1>=vEdges[i].Ind1 && !vEdges[j].Head1)
					Anch1Tail.push_back(vEdges[j].Ind2);
				if(vEdges[j].Ind2<=vEdges[i].Ind2 && vEdges[j].Head2)
					Anch2Head.push_back(vEdges[j].Ind1);
				else if(vEdges[j].Ind2>=vEdges[i].Ind2 && !vEdges[j].Head2)
					Anch2Tail.push_back(vEdges[j].Ind1);
			}
		}
		pair<int,int> E1Head, E1Tail, E2Head, E2Tail;
		int Mean1Head=0, Mean1Tail=0, Mean2Head=0, Mean2Tail=0;
		if(Anch1Head.size()!=0){
			E1Head=ExtremeValue(Anch1Head.begin(), Anch1Head.end());
			Mean1Head=1.0*(E1Head.first+E1Head.second)/2;
		}
		if(Anch1Tail.size()!=0){
			E1Tail=ExtremeValue(Anch1Tail.begin(), Anch1Tail.end());
			Mean1Tail=1.0*(E1Tail.first+E1Tail.second)/2;
		}
		if(Anch2Head.size()!=0){
			E2Head=ExtremeValue(Anch2Head.begin(), Anch2Head.end());
			Mean2Head=1.0*(E2Head.first+E2Head.second)/2;
		}
		if(Anch2Tail.size()!=0){
			E2Tail=ExtremeValue(Anch2Tail.begin(), Anch2Tail.end());
			Mean2Tail=1.0*(E2Tail.first+E2Tail.second)/2;
		}
		/*if((E1Head.first<=E1Tail.first && E1Head.second-E1Tail.first>1) || (E1Tail.first<=E1Head.first && E1Tail.second-E1Head.first>1))
			whetherdelete=true;
		else if((E2Head.first<=E2Tail.first && E2Head.second-E2Tail.first>1) || (E2Tail.first<=E2Head.first && E2Tail.second-E2Head.first>1))
			whetherdelete=true;*/
		if(!whetherdelete){
			for(int j=i-1; j>-1 && vEdges[j].Ind1>=vEdges[i].Ind1-Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length>=pos1-Concord_Dist_Pos; j--){
				int newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
				int newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
				if(vEdges[j].Ind2>vEdges[i].Ind1 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos){
					if(vEdges[j].Ind1<vEdges[i].Ind1 && !vEdges[j].Head1 && abs(vEdges[j].Ind2-Mean1Head)<abs(vEdges[j].Ind2-Mean1Tail) && Anch1Head.size()!=0 && Anch1Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					else if(vEdges[j].Ind1>vEdges[i].Ind1 && vEdges[j].Head1 && abs(vEdges[j].Ind2-Mean1Tail)<abs(vEdges[j].Ind2-Mean1Head) && Anch1Head.size()!=0 && Anch1Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					if(vEdges[j].Ind2<vEdges[i].Ind2 && !vEdges[j].Head2 && abs(vEdges[j].Ind1-Mean2Head)<abs(vEdges[j].Ind1-Mean2Tail) && Anch2Head.size()!=0 && Anch2Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					else if(vEdges[j].Ind2>vEdges[i].Ind2 && vEdges[j].Head2 && abs(vEdges[j].Ind1-Mean2Tail)<abs(vEdges[j].Ind1-Mean2Head) && Anch2Head.size()!=0 && Anch2Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
				}
			}
		}
		if(!whetherdelete){
			for(int j=i+1; j<vEdges.size() && vEdges[j].Ind1<=vEdges[i].Ind1+Concord_Dist_Idx && vNodes[vEdges[j].Ind1].Chr==chr1 && vNodes[vEdges[j].Ind1].Position<=pos1+Concord_Dist_Pos; j++){
				int newpos1=(vEdges[j].Head1)? vNodes[vEdges[j].Ind1].Position:(vNodes[vEdges[j].Ind1].Position+vNodes[vEdges[j].Ind1].Length);
				int newpos2=(vEdges[j].Head2)? vNodes[vEdges[j].Ind2].Position:(vNodes[vEdges[j].Ind2].Position+vNodes[vEdges[j].Ind2].Length);
				if(vEdges[j].Ind1<vEdges[i].Ind2 && abs(vEdges[j].Ind2-vEdges[i].Ind2)<=Concord_Dist_Idx && abs(newpos1-pos1)<=Concord_Dist_Pos && abs(newpos2-pos2)<=Concord_Dist_Pos){
					if(vEdges[j].Ind1<vEdges[i].Ind1 && !vEdges[j].Head1 && abs(vEdges[j].Ind2-Mean1Head)<abs(vEdges[j].Ind2-Mean1Tail) && Anch1Head.size()!=0 && Anch1Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					else if(vEdges[j].Ind1>vEdges[i].Ind1 && vEdges[j].Head1 && abs(vEdges[j].Ind2-Mean1Tail)<abs(vEdges[j].Ind2-Mean1Head) && Anch1Head.size()!=0 && Anch1Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					if(vEdges[j].Ind2<vEdges[i].Ind2 && !vEdges[j].Head2 && abs(vEdges[j].Ind1-Mean2Head)<abs(vEdges[j].Ind1-Mean2Tail) && Anch2Head.size()!=0 && Anch2Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
					else if(vEdges[j].Ind2>vEdges[i].Ind2 && vEdges[j].Head2 && abs(vEdges[j].Ind1-Mean2Tail)<abs(vEdges[j].Ind1-Mean2Head) && Anch2Head.size()!=0 && Anch2Tail.size()!=0)
						whetherdelete=true; //XXX is comparison of abs should include =?
				}
			}
		}
		if(whetherdelete)
			ImpossibleEdges.insert(ImpossibleEdges.end(), PotentialEdges.begin(), PotentialEdges.end());
	}
	sort(ImpossibleEdges.begin(), ImpossibleEdges.end());
	vector<Edge_t>::iterator it=set_difference(vEdges.begin(), vEdges.end(), ImpossibleEdges.begin(), ImpossibleEdges.end(), tmpEdges.begin());
	tmpEdges.resize(distance(tmpEdges.begin(), it));
	vEdges=tmpEdges;
	UpdateNodeLink();
};

int SegmentGraph_t::GroupConnection(int node, vector<Edge_t*>& Edges, int sumweight, vector<int>& Connection, vector<int>& Label){
	Connection.clear();
	int count=0, mindist=-1, index=-1;
	for(vector<Edge_t*>::iterator it=Edges.begin(); it!=Edges.end(); it++)
		if((*it)->GroupWeight>0.01*sumweight || (*it)->GroupWeight>Min_Edge_Weight)
			Connection.push_back(((*it)->Ind1!=node)?(*it)->Ind1:(*it)->Ind2);
	sort(Connection.begin(), Connection.end());
	Label.assign(Connection.size(), -1);
	for(int i=0; i<Connection.size(); i++)
		if(vNodes[Connection[i]].Chr==vNodes[node].Chr && vNodes[node].Position-vNodes[Connection[i]].Position-vNodes[Connection[i]].Length<=Concord_Dist_Pos && vNodes[Connection[i]].Position-vNodes[node].Position-vNodes[node].Length<=Concord_Dist_Pos){
			if(mindist==-1 || mindist>abs(node-Connection[i])){
				mindist=abs(node-Connection[i]);
				index=i;
			}
		}
	if(index!=-1){
		Label[index]=0;
		for(int i=index+1; i<Connection.size(); i++)
			if(vNodes[Connection[i]].Chr==vNodes[node].Chr && vNodes[Connection[i]].Position-vNodes[Connection[i-1]].Position-vNodes[Connection[i-1]].Length<=Concord_Dist_Pos)
				Label[i]=0;
			else
				break;
		for(int i=index-1; i>=0; i--)
			if(vNodes[Connection[i]].Chr==vNodes[node].Chr && vNodes[Connection[i+1]].Position-vNodes[Connection[i]].Position-vNodes[Connection[i]].Length<=Concord_Dist_Pos)
				Label[i]=0;
			else
				break;
	}
	if(Label.size()!=0){
		count=(Label[0]==-1)?1:0;
		if(Label[0]==-1)
			Label[0]=1;
		for(int i=1; i<Connection.size(); i++){
			if(Label[i]!=-1)
				continue;
			else if(vNodes[Connection[i]].Chr!=vNodes[Connection[i-1]].Chr || vNodes[Connection[i]].Position-vNodes[Connection[i-1]].Position-vNodes[Connection[i-1]].Length>Concord_Dist_Pos){
				count++;
			}
			Label[i]=count;
		}
	}
	return count;
};

void SegmentGraph_t::GroupSelect(int node, vector<Edge_t*>& Edges, int sumweight, int count, vector<int>& Connection, vector<int>& Label, vector<Edge_t>& ToDelete){
	vector<int> LabelWeight(count+1, 0);
	for(vector<Edge_t*>::iterator it=Edges.begin(); it!=Edges.end(); it++)
		if((*it)->GroupWeight>0.01*sumweight || (*it)->GroupWeight>Min_Edge_Weight){
			int mateNode=((*it)->Ind1!=node)?(*it)->Ind1:(*it)->Ind2;
			int mateNodeidx=distance(Connection.begin(), find(Connection.begin(), Connection.end(), mateNode));
			LabelWeight[Label[mateNodeidx]]+=(*it)->Weight;
		}
	int maxLabel=1;
	for(int i=1; i<LabelWeight.size(); i++)
		if(LabelWeight[i]>LabelWeight[maxLabel])
			maxLabel=i;
	for(vector<Edge_t*>::iterator it=Edges.begin(); it!=Edges.end(); it++)
		if((*it)->GroupWeight>0.01*sumweight || (*it)->GroupWeight>Min_Edge_Weight){
			int mateNode=((*it)->Ind1!=node)?(*it)->Ind1:(*it)->Ind2;
			int mateNodeidx=distance(Connection.begin(), find(Connection.begin(), Connection.end(), mateNode));
			if(Label[mateNodeidx]!=maxLabel && Label[mateNodeidx]!=0)
				ToDelete.push_back(*(*it));
		}
};

void SegmentGraph_t::FilterEdges(){
	vector<int> BadNodes;
	vector<Edge_t> ToDelete;
	for(int i=0; i<vNodes.size(); i++){
		int sumweight=0;
		for(vector<Edge_t*>::iterator it=vNodes[i].HeadEdges.begin(); it!=vNodes[i].HeadEdges.end(); it++)
			sumweight+=(*it)->Weight;
		for(vector<Edge_t*>::iterator it=vNodes[i].TailEdges.begin(); it!=vNodes[i].TailEdges.end(); it++)
			sumweight+=(*it)->Weight;
		vector<int> tmp;
		for(vector<Edge_t*>::iterator it=vNodes[i].HeadEdges.begin(); it!=vNodes[i].HeadEdges.end(); it++){
			if((*it)->GroupWeight<=0.01*sumweight && (*it)->GroupWeight<=Min_Edge_Weight)
				ToDelete.push_back(*(*it));
		}
		for(vector<Edge_t*>::iterator it=vNodes[i].TailEdges.begin(); it!=vNodes[i].TailEdges.end(); it++){
			if((*it)->GroupWeight<=0.01*sumweight && (*it)->GroupWeight<=Min_Edge_Weight)
				ToDelete.push_back(*(*it));
		}
		vector<int> HeadConn, TailConn;
		vector<int> HeadLabel, TailLabel;
		int headcount=0, tailcount=0;
		if(vNodes[i].HeadEdges.size()!=0)
			headcount=GroupConnection(i, vNodes[i].HeadEdges, sumweight, HeadConn, HeadLabel);
		if(vNodes[i].TailEdges.size()!=0)
			tailcount=GroupConnection(i, vNodes[i].TailEdges, sumweight, TailConn, TailLabel);
		if(headcount+tailcount>5)
			BadNodes.push_back(i);
		else{
			if(headcount>1)
				GroupSelect(i, vNodes[i].HeadEdges, sumweight, headcount, HeadConn, HeadLabel, ToDelete);
			if(tailcount>1)
				GroupSelect(i, vNodes[i].TailEdges, sumweight, tailcount, TailConn, TailLabel, ToDelete);
		}
	}
	sort(ToDelete.begin(), ToDelete.end());
	sort(BadNodes.begin(), BadNodes.end());
	vector<Edge_t> tmpEdges; tmpEdges.reserve(vEdges.size());
	for(int i=0; i<vEdges.size(); i++){
		bool cond1=false, cond2=true;
		if(!binary_search(BadNodes.begin(), BadNodes.end(), vEdges[i].Ind1) && !binary_search(BadNodes.begin(), BadNodes.end(), vEdges[i].Ind2) && vEdges[i].GroupWeight>Min_Edge_Weight)
			cond1=true;
		else if(vNodes[vEdges[i].Ind1].Chr==vNodes[vEdges[i].Ind2].Chr && abs(vNodes[vEdges[i].Ind2].Position-vNodes[vEdges[i].Ind1].Position-vNodes[vEdges[i].Ind1].Length)<=Concord_Dist_Pos && vEdges[i].GroupWeight>Min_Edge_Weight)
			cond1=true;
		if(cond1 && (vEdges[i].Ind2-vEdges[i].Ind1>Concord_Dist_Idx || vEdges[i].Head1!=false || vEdges[i].Head2!=true)){
			double cov1=vNodes[vEdges[i].Ind1].AvgDepth;
			double cov2=vNodes[vEdges[i].Ind2].AvgDepth;
			double ratio=(cov1>cov2)?cov1/cov2:cov2/cov1;
			if((vEdges[i].Weight<=10 && ratio>3) || (vEdges[i].Weight>10 && ratio>50))
				cond2=false;
		}
		if(cond1 && cond2)
			tmpEdges.push_back(vEdges[i]);
	}
	tmpEdges.reserve(tmpEdges.size());
	sort(tmpEdges.begin(), tmpEdges.end());
	vector<Edge_t>::iterator it=set_difference(tmpEdges.begin(), tmpEdges.end(), ToDelete.begin(), ToDelete.end(), vEdges.begin());
	vEdges.resize(distance(vEdges.begin(), it));
	UpdateNodeLink();
};

void SegmentGraph_t::CompressNode(){
	// find all nodes that have edges connected.
	vector<int> LinkedNode; LinkedNode.reserve(vEdges.size()*2);
	for(vector<Edge_t>::iterator it=vEdges.begin(); it!=vEdges.end(); it++){
		LinkedNode.push_back(it->Ind1);
		LinkedNode.push_back(it->Ind2);
	}
	sort(LinkedNode.begin(), LinkedNode.end());
	vector<int>::iterator endit=unique(LinkedNode.begin(), LinkedNode.end());
	LinkedNode.resize(distance(LinkedNode.begin(), endit));
	LinkedNode.reserve(LinkedNode.size());
	// merge consecutive unlinked nodes, and make new Node_t vector newNodes
	vector<Node_t> newNodes; newNodes.reserve(vNodes.size());
	int count=0;
	std::map<int, int> LinkedOld_New;
	for(int i=0; i<LinkedNode.size(); i++){
		int startidx=(i==0)?0:(LinkedNode[i-1]+1), endidx=LinkedNode[i], lastinsert;
		lastinsert=startidx;
		for(int j=startidx; j<endidx; j++){
			if(vNodes[j].Chr!=vNodes[lastinsert].Chr){
				Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[j-1].Position+vNodes[j-1].Length-vNodes[lastinsert].Position, 0);
				for(int k=lastinsert; k<j; k++){
					tmp.Support+=vNodes[k].Support;
					tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
				}
				tmp.AvgDepth/=tmp.Length;
				newNodes.push_back(tmp); count++;
				lastinsert=j;
			}
		}
		if(lastinsert!=endidx){
			Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[endidx-1].Position+vNodes[endidx-1].Length-vNodes[lastinsert].Position, 0);
			for(int k=lastinsert; k<endidx; k++){
				tmp.Support+=vNodes[k].Support;
				tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
			}
			tmp.AvgDepth/=tmp.Length;
			newNodes.push_back(tmp); count++;
		}
		newNodes.push_back(vNodes[endidx]); count++;
		LinkedOld_New[endidx]=count-1;
	}
	if(LinkedNode.back()!=vNodes.size()-1){
		int startidx=LinkedNode.back()+1, endidx=vNodes.size(), lastinsert;
		lastinsert=startidx;
		for(int j=startidx; j<endidx; j++){
			if(vNodes[j].Chr!=vNodes[lastinsert].Chr){
				Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[j-1].Position+vNodes[j-1].Length-vNodes[lastinsert].Position, 0);
				for(int k=lastinsert; k<j; k++){
					tmp.Support+=vNodes[k].Support;
					tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
				}
				tmp.AvgDepth/=tmp.Length;
				newNodes.push_back(tmp); count++;
				lastinsert=j;
			}
		}
		if(lastinsert!=endidx){
			Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[endidx-1].Position+vNodes[endidx-1].Length-vNodes[lastinsert].Position, 0);
			for(int k=lastinsert; k<endidx; k++){
				tmp.Support+=vNodes[k].Support;
				tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
			}
			tmp.AvgDepth/=tmp.Length;
			newNodes.push_back(tmp); count++;
		}
	}
	for(vector<Edge_t>::iterator it=vEdges.begin(); it!=vEdges.end(); it++){
		it->Ind1=LinkedOld_New[it->Ind1];
		it->Ind2=LinkedOld_New[it->Ind2];
	}
	vNodes=newNodes;
	UpdateNodeLink();
};

void SegmentGraph_t::CompressNode(vector< vector<int> >& Read_Node){
	// find all nodes that have edges connected.
	vector<int> LinkedNode; LinkedNode.reserve(vEdges.size()*2);
	for(vector<Edge_t>::iterator it=vEdges.begin(); it!=vEdges.end(); it++){
		LinkedNode.push_back(it->Ind1);
		LinkedNode.push_back(it->Ind2);
	}
	sort(LinkedNode.begin(), LinkedNode.end());
	vector<int>::iterator endit=unique(LinkedNode.begin(), LinkedNode.end());
	LinkedNode.resize(distance(LinkedNode.begin(), endit));
	LinkedNode.reserve(LinkedNode.size());
	// merge consecutive unlinked nodes, and make new Node_t vector newNodes
	vector<Node_t> newNodes; newNodes.reserve(vNodes.size());
	int count=0;
	std::map<int, int> LinkedOld_New;
	for(int i=0; i<LinkedNode.size(); i++){
		int startidx=(i==0)?0:(LinkedNode[i-1]+1), endidx=LinkedNode[i], lastinsert;
		lastinsert=startidx;
		for(int j=startidx; j<endidx; j++){
			if(vNodes[j].Chr!=vNodes[lastinsert].Chr){
				Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[j-1].Position+vNodes[j-1].Length-vNodes[lastinsert].Position, 0);
				for(int k=lastinsert; k<j; k++){
					tmp.Support+=vNodes[k].Support;
					tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
				}
				tmp.AvgDepth/=tmp.Length;
				newNodes.push_back(tmp); count++;
				lastinsert=j;
			}
		}
		if(lastinsert!=endidx){
			Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[endidx-1].Position+vNodes[endidx-1].Length-vNodes[lastinsert].Position, 0);
			for(int k=lastinsert; k<endidx; k++){
				tmp.Support+=vNodes[k].Support;
				tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
			}
			tmp.AvgDepth/=tmp.Length;
			newNodes.push_back(tmp); count++;
		}
		newNodes.push_back(vNodes[endidx]); count++;
		LinkedOld_New[endidx]=count-1;
	}
	if(LinkedNode.back()!=vNodes.size()-1){
		int startidx=LinkedNode.back()+1, endidx=vNodes.size(), lastinsert;
		lastinsert=startidx;
		for(int j=startidx; j<endidx; j++){
			if(vNodes[j].Chr!=vNodes[lastinsert].Chr){
				Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[j-1].Position+vNodes[j-1].Length-vNodes[lastinsert].Position, 0);
				for(int k=lastinsert; k<j; k++){
					tmp.Support+=vNodes[k].Support;
					tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
				}
				tmp.AvgDepth/=tmp.Length;
				newNodes.push_back(tmp); count++;
				lastinsert=j;
			}
		}
		if(lastinsert!=endidx){
			Node_t tmp(vNodes[lastinsert].Chr, vNodes[lastinsert].Position, vNodes[endidx-1].Position+vNodes[endidx-1].Length-vNodes[lastinsert].Position, 0);
			for(int k=lastinsert; k<endidx; k++){
				tmp.Support+=vNodes[k].Support;
				tmp.AvgDepth+=vNodes[k].AvgDepth*vNodes[k].Length;
			}
			tmp.AvgDepth/=tmp.Length;
			newNodes.push_back(tmp); count++;
		}
	}
	for(vector<Edge_t>::iterator it=vEdges.begin(); it!=vEdges.end(); it++){
		it->Ind1=LinkedOld_New[it->Ind1];
		it->Ind2=LinkedOld_New[it->Ind2];
	}
	vNodes=newNodes;
	UpdateNodeLink();
	for(int i=0; i<Read_Node.size(); i++)
		for(int j=0; j<Read_Node[i].size(); j++){
			if(Read_Node[i][j]==-1)
				continue;
			vector<int>::iterator lb=lower_bound(LinkedNode.begin(), LinkedNode.end(), Read_Node[i][j]);
			if((*lb)==Read_Node[i][j])
				Read_Node[i][j]=LinkedOld_New[(*lb)];
			else if((*lb)<Read_Node[i][j])
				Read_Node[i][j]=LinkedOld_New[(*lb)]+1;
			else // lower bound is larger only when all number in LinkedNode are larger than Read_Node[i][j], and the first element is returned
				Read_Node[i][j]=LinkedOld_New[(*lb)]-1;
		}
};

void SegmentGraph_t::FurtherCompressNode(){
	vector<int> MergeNode(vNodes.size(), -1);
	int curNode=0;
	int rightmost=0;
	for(int i=0; i<MergeNode.size(); i++){
		int minDisInd2;
		vector<Edge_t> thisDiscordantEdges, tmpDiscordantEdges;
		if(i!=0 && vNodes[i].Chr!=vNodes[i-1].Chr && curNode==MergeNode[i-1])
			curNode++;
		for(vector<Edge_t*>::iterator itedge=vNodes[i].HeadEdges.begin(); itedge!=vNodes[i].HeadEdges.end(); itedge++){
			if(IsDiscordant(*itedge))
				thisDiscordantEdges.push_back(**itedge);
			else
				rightmost=(rightmost>max((*itedge)->Ind1, (*itedge)->Ind2))?rightmost:max((*itedge)->Ind1, (*itedge)->Ind2);
		}
		for(vector<Edge_t*>::iterator itedge=vNodes[i].TailEdges.begin(); itedge!=vNodes[i].TailEdges.end(); itedge++){
			if(IsDiscordant(*itedge))
				thisDiscordantEdges.push_back(**itedge);
			else
				rightmost=(rightmost>max((*itedge)->Ind1, (*itedge)->Ind2))?rightmost:max((*itedge)->Ind1, (*itedge)->Ind2);
		}
		if(thisDiscordantEdges.size()!=0){ // remove discordant edges that are in the same group
			minDisInd2 = (thisDiscordantEdges[0].Ind1==i)?thisDiscordantEdges[0].Ind2:(i+20);
			tmpDiscordantEdges.push_back(thisDiscordantEdges[0]);
			for(int k=0; k+1<thisDiscordantEdges.size(); k++){
				const Edge_t& edge1= thisDiscordantEdges[k];
				const Edge_t& edge2= thisDiscordantEdges[k+1];
				bool samegroup = ((edge1.Ind1==i && edge2.Ind1==i) || (edge1.Ind2==i && edge2.Ind2==i));
				if(!(abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2))
					samegroup=false;
				if(!samegroup)
					tmpDiscordantEdges.push_back(thisDiscordantEdges[k+1]);
				int tmpmin = (edge2.Ind1==i)?edge2.Ind2:(i+20);
				if(tmpmin<minDisInd2)
					minDisInd2=tmpmin;
			}
			thisDiscordantEdges=tmpDiscordantEdges;
		}

		if(MergeNode[i]==-1){
			if(thisDiscordantEdges.size()==0 && i<rightmost) // node with only concordant edges, and further connect right nodes
				MergeNode[i]=curNode;
			else if(thisDiscordantEdges.size()==0 && i==rightmost){ // node with only concordant edges, and doesn't connect right nodes
				MergeNode[i]=curNode;
				curNode++;
				rightmost++;
			}
			else{ // first node with discordant in its equivalent group
				if(i!=0 && curNode==MergeNode[i-1])
					curNode++;
				vector<Edge_t> nextDiscordantEdges;
				int j=i+1;
				for(; j<MergeNode.size() && j<i+20 && j<minDisInd2; j++){ // find the first right nodes with discordant edges, possibly in the equivalent group
					for(vector<Edge_t*>::iterator itedge=vNodes[j].HeadEdges.begin(); itedge!=vNodes[j].HeadEdges.end(); itedge++)
						if(IsDiscordant(*itedge))
							nextDiscordantEdges.push_back(**itedge);
					for(vector<Edge_t*>::iterator itedge=vNodes[j].TailEdges.begin(); itedge!=vNodes[j].TailEdges.end(); itedge++)
						if(IsDiscordant(*itedge))
							nextDiscordantEdges.push_back(**itedge);
					if(nextDiscordantEdges.size()!=0)
						break;
				}
				bool equivalent = (nextDiscordantEdges.size()!=0); // check whether indeed in the same equivalent group
				if(nextDiscordantEdges.size()!=0){
					tmpDiscordantEdges.clear();
					tmpDiscordantEdges.push_back(nextDiscordantEdges[0]);
					for(int k=0; k+1<nextDiscordantEdges.size(); k++){
						const Edge_t& edge1=nextDiscordantEdges[k];
						const Edge_t& edge2=nextDiscordantEdges[k+1];
						bool samegroup = ((edge1.Ind1==j && edge2.Ind1==j) || (edge1.Ind2==j && edge2.Ind2==j));
						if(!(abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2))
							samegroup=false;
						if(!samegroup)
							tmpDiscordantEdges.push_back(nextDiscordantEdges[k+1]);
					}
					nextDiscordantEdges=tmpDiscordantEdges;
					tmpDiscordantEdges.clear();
					vector<bool> thisEQ(thisDiscordantEdges.size(), false);
					vector<bool> nextEQ(nextDiscordantEdges.size(), false);
					for(int k=0; k<thisDiscordantEdges.size(); k++)
						for(int l=0; l<nextDiscordantEdges.size(); l++){
							const Edge_t& edge1=thisDiscordantEdges[k];
							const Edge_t& edge2=nextDiscordantEdges[l];
							if(edge1.Ind2>edge2.Ind1 && edge2.Ind2>edge1.Ind1 && abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2){
								thisEQ[k]=true;
								nextEQ[l]=true;
							}
						}
					for(int k=0; k<thisEQ.size(); k++)
						if(!thisEQ[k])
							equivalent=false;
					for(int l=0; l<nextEQ.size(); l++)
						if(!nextEQ[l])
							equivalent=false;
				}
				if(!equivalent){ // equivalend group has only the current item
					MergeNode[i]=curNode;
					curNode++;
				}
				else{ // equivalent group has other items than current item
					for(int k=i; k<=j; k++)
						MergeNode[k]=curNode;
				}
				rightmost=i+1;
			}
		}
		else if(thisDiscordantEdges.size()!=0){ // later nodes with discordant edges in its equivalent group
			vector<Edge_t> nextDiscordantEdges;
			int j=i+1;
			for(; j<MergeNode.size() && j<i+20 && j<minDisInd2; j++){
				for(vector<Edge_t*>::iterator itedge=vNodes[j].HeadEdges.begin(); itedge!=vNodes[j].HeadEdges.end(); itedge++)
					if(IsDiscordant(*itedge))
						nextDiscordantEdges.push_back(**itedge);
				for(vector<Edge_t*>::iterator itedge=vNodes[j].TailEdges.begin(); itedge!=vNodes[j].TailEdges.end(); itedge++)
					if(IsDiscordant(*itedge))
						nextDiscordantEdges.push_back(**itedge);
				if(nextDiscordantEdges.size()!=0)
					break;
			}
			bool equivalent = (nextDiscordantEdges.size()!=0);
			if(nextDiscordantEdges.size()!=0){
				tmpDiscordantEdges.clear();
				tmpDiscordantEdges.push_back(thisDiscordantEdges[0]);
				for(int k=0; k+1<thisDiscordantEdges.size(); k++){
					const Edge_t& edge1= thisDiscordantEdges[k];
					const Edge_t& edge2= thisDiscordantEdges[k+1];
					if(!(abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2))
						tmpDiscordantEdges.push_back(thisDiscordantEdges[k+1]);
				}
				thisDiscordantEdges=tmpDiscordantEdges;
				tmpDiscordantEdges.clear();
				tmpDiscordantEdges.push_back(nextDiscordantEdges[0]);
				for(int k=0; k+1<nextDiscordantEdges.size(); k++){
					const Edge_t& edge1=nextDiscordantEdges[k];
					const Edge_t& edge2=nextDiscordantEdges[k+1];
					if(!(abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2))
						tmpDiscordantEdges.push_back(nextDiscordantEdges[k+1]);
				}
				nextDiscordantEdges=tmpDiscordantEdges;
				tmpDiscordantEdges.clear();
				vector<bool> thisEQ(thisDiscordantEdges.size(), false);
				vector<bool> nextEQ(nextDiscordantEdges.size(), false);
				for(int k=0; k<thisDiscordantEdges.size(); k++)
					for(int l=0; l<nextDiscordantEdges.size(); l++){
						const Edge_t& edge1=thisDiscordantEdges[k];
						const Edge_t& edge2=nextDiscordantEdges[l];
						if(edge1.Ind2>edge2.Ind1 && edge2.Ind2>edge1.Ind1 && abs(edge1.Ind1-edge2.Ind1)<=Concord_Dist_Idx && abs(edge1.Ind2-edge2.Ind2)<=Concord_Dist_Idx && edge1.Head1==edge2.Head1 && edge1.Head2==edge2.Head2){
							thisEQ[k]=true;
							nextEQ[l]=true;
						}
					}
				for(int k=0; k<thisEQ.size(); k++)
					if(!thisEQ[k])
						equivalent=false;
				for(int l=0; l<nextEQ.size(); l++)
					if(!nextEQ[l])
						equivalent=false;
			}
			if(!equivalent) // not able to find the next one with discordant edges in the same equivalent group
				curNode++;
			else{
				for(int k=i; k<=j; k++)
					MergeNode[k]=curNode;
			}
			rightmost=i+1;
		}
	}

	for(int i=0; i+1<MergeNode.size(); i++)
		assert((MergeNode[i]==MergeNode[i+1]) || (MergeNode[i]+1==MergeNode[i+1]));

	vector<Node_t> newvNodes; newvNodes.reserve(curNode);
	vector<Edge_t> newvEdges; newvEdges.reserve(vEdges.size());
	int ind=0;
	while(ind<MergeNode.size()){
		int j=ind;
		for(; j<MergeNode.size() && MergeNode[j]==MergeNode[ind]; j++){}
		Node_t tmpnode(vNodes[ind].Chr, vNodes[ind].Position, vNodes[j-1].Position+vNodes[j-1].Length-vNodes[ind].Position);
		newvNodes.push_back(tmpnode);
		ind=j;
	}
	for(int i=0; i<vEdges.size(); i++){
		const Edge_t& edge=vEdges[i];
		if(MergeNode[edge.Ind1]!=MergeNode[edge.Ind2]){
			Edge_t tmpedge(MergeNode[edge.Ind1], edge.Head1, MergeNode[edge.Ind2], edge.Head2, edge.Weight);
			newvEdges.push_back(tmpedge);
		}
	}

	vNodes=newvNodes;
	vEdges.clear();
	sort(newvEdges.begin(), newvEdges.end());
	for(int i=0; i<newvEdges.size(); i++){
		if(vEdges.size()==0 || !(newvEdges[i]==vEdges.back()))
			vEdges.push_back(newvEdges[i]);
		else
			vEdges.back().Weight+=newvEdges[i].Weight;
	}
	UpdateNodeLink();
};

void SegmentGraph_t::UpdateNodeLink(){
	for(vector<Node_t>::iterator it=vNodes.begin(); it!=vNodes.end(); it++){
		it->HeadEdges.clear();
		it->TailEdges.clear();
	}
	for(vector<Edge_t>::iterator it=vEdges.begin(); it!=vEdges.end(); it++){
		if(it->Head1)
			vNodes[it->Ind1].HeadEdges.push_back(&(*it));
		else
			vNodes[it->Ind1].TailEdges.push_back(&(*it));
		if(it->Head2)
			vNodes[it->Ind2].HeadEdges.push_back(&(*it));
		else
			vNodes[it->Ind2].TailEdges.push_back(&(*it));  
	}
};

int SegmentGraph_t::DFS(int node, int curlabelid, vector<int>& Label){
	int componentsize=0;
	vector<int> Unvisited;
	Unvisited.push_back(node);
	while(Unvisited.size()!=0){
		int v=Unvisited.back(); Unvisited.pop_back();
		if(Label[v]==-1){
			Label[v]=curlabelid;
			componentsize++;
			for(int i=0; i<vNodes[v].HeadEdges.size(); i++){
				if(vNodes[v].HeadEdges[i]->Ind1!=v)
					Unvisited.push_back(vNodes[v].HeadEdges[i]->Ind1);
				else if(vNodes[v].HeadEdges[i]->Ind2!=v)
					Unvisited.push_back(vNodes[v].HeadEdges[i]->Ind2);
			}
			for(int i=0; i<vNodes[v].TailEdges.size(); i++){
				if(vNodes[v].TailEdges[i]->Ind1!=v)
					Unvisited.push_back(vNodes[v].TailEdges[i]->Ind1);
				else if(vNodes[v].TailEdges[i]->Ind2!=v)
					Unvisited.push_back(vNodes[v].TailEdges[i]->Ind2);
			}
		}
	}
	return componentsize;
};

void SegmentGraph_t::OutputDegree(string outputfile){
	ofstream output(outputfile, ios::out);
	output<<"# node_id\ttotaldegree\tfarawaydegree(5)\n";
	for(int i=0; i<vNodes.size(); i++){
		vector<int> tmp;
		for(vector<Edge_t*>::iterator it=vNodes[i].HeadEdges.begin(); it!=vNodes[i].HeadEdges.end(); it++){
			if((*it)->Ind1!=i)
				tmp.push_back((*it)->Ind1);
			if((*it)->Ind2!=i)
				tmp.push_back((*it)->Ind2);
		}
		for(vector<Edge_t*>::iterator it=vNodes[i].TailEdges.begin(); it!=vNodes[i].TailEdges.end(); it++){
			if((*it)->Ind1!=i)
				tmp.push_back((*it)->Ind1);
			if((*it)->Ind2!=i)
				tmp.push_back((*it)->Ind2);
		}
		sort(tmp.begin(), tmp.end());
		vector<int>::iterator endit=unique(tmp.begin(), tmp.end());
		tmp.resize(distance(tmp.begin(), endit));
		int count=0;
		for(int j=1; j<tmp.size(); j++)
			if(tmp[j]-tmp[j-1]>5)
				count++;
		output<<i<<'\t'<<(tmp.size())<<'\t'<<count<<endl;
	}
	output.close();
};

void SegmentGraph_t::ConnectedComponent(int & maxcomponentsize){
	Label.clear();
	Label.resize(vNodes.size(), -1);
	int numlabeled=0, curlabelid=0;
	maxcomponentsize=0;
	while(numlabeled<vNodes.size()){
		for(int i=0; i<vNodes.size(); i++){
			if(Label[i]==-1){
				int curcomponentsize=DFS(i, curlabelid, Label);
				if(curcomponentsize>maxcomponentsize)
					maxcomponentsize=curcomponentsize;
				numlabeled+=curcomponentsize;
				break;
			}
		}
		curlabelid++;
	}
	cout<<"Maximum connected component size="<<maxcomponentsize<<endl;
};

void SegmentGraph_t::ConnectedComponent(){
	Label.clear();
	Label.resize(vNodes.size(), -1);
	int numlabeled=0, curlabelid=0, maxcomponentsize=0;
	while(numlabeled<vNodes.size()){
		for(int i=0; i<vNodes.size(); i++){
			if(Label[i]==-1){
				int curcomponentsize=DFS(i, curlabelid, Label);
				if(curcomponentsize>maxcomponentsize)
					maxcomponentsize=curcomponentsize;
				numlabeled+=curcomponentsize;
				break;
			}
		}
		curlabelid++;
	}
	cout<<"Maximum connected component size="<<maxcomponentsize<<endl;
};

void SegmentGraph_t::OutputGraph(string outputfile){
	ofstream output(outputfile, ios::out);
	output<<"# type=node\tid\tChr\tPosition\tEnd\tSupport\tAvgDepth\tLabel\n";
	output<<"# type=edge\tid\tInd1\tHead1\tInd2\tHead2\tWeight\n";
	for(int i=0; i<vNodes.size(); i++){
		output<<"node\t"<<i<<'\t'<<vNodes[i].Chr<<'\t'<<vNodes[i].Position<<'\t'<<(vNodes[i].Position+vNodes[i].Length)<<'\t'<<vNodes[i].Support<<'\t'<<vNodes[i].AvgDepth<<'\t'<<Label[i]<<'\n';
	}
	for(int i=0; i<vEdges.size(); i++){
		output<<"edge\t"<<i<<'\t'<<vEdges[i].Ind1<<'\t'<<(vEdges[i].Head1?"H\t":"T\t")<<vEdges[i].Ind2<<'\t'<<(vEdges[i].Head2?"H\t":"T\t")<<vEdges[i].Weight<<endl;
	}
	output.close();
};

vector< vector<int> > SegmentGraph_t::Ordering(){
	int componentsize=0;
	for(int i=0; i<Label.size(); i++)
		if(Label[i]>componentsize)
			componentsize=Label[i];
	componentsize++;
	vector< vector<int> > BestOrders; BestOrders.resize(componentsize);
	for(int i=0; i<componentsize; i++){
		std::map<int,int> CompNodes;
		vector<Edge_t> CompEdges;
		// select vNodes and vEdges of corresponding component
		int count=0;
		for(int j=0; j<Label.size(); j++)
			if(Label[j]==i)
				CompNodes[j]=count++;
		for(int j=0; j<vEdges.size(); j++)
			if((Label[vEdges[j].Ind1]==i || Label[vEdges[j].Ind2]==i) && vEdges[j].Ind1!=vEdges[j].Ind2)
				CompEdges.push_back(vEdges[j]);
		if(CompNodes.size()==1){
			BestOrders[i].push_back(CompNodes.begin()->first+1);
			continue;
		}
		//cout<<"component "<<i<<endl;
		BestOrders[i]=MincutRecursion(CompNodes, CompEdges);
	}
	return BestOrders;
};

vector<int> SegmentGraph_t::MincutRecursion(std::map<int,int> CompNodes, vector<Edge_t> CompEdges){
	if(CompNodes.size()==1){
		vector<int> BestOrder;
		std::map<int,int>::iterator it=CompNodes.begin();
		BestOrder.push_back(it->first+1);
		return BestOrder;
	}
	else if(CompNodes.size()<40){
		vector<int> BestOrder(CompNodes.size(), 0);
		GRBEnv env=GRBEnv();;
		GRBModel model=GRBModel(env);
		model.getEnv().set(GRB_IntParam_LogToConsole, 0);
		model.getEnv().set(GRB_DoubleParam_TimeLimit, 300.0);
		vector<GRBVar> vGRBVar;
		int edgeidx=0;
		std::map<int,int>::iterator itnodeend=CompNodes.end(); itnodeend--;
		for(std::map<int,int>::iterator itnode=CompNodes.begin(); itnode!=itnodeend; itnode++){
			bool isfound=false;
			for(; edgeidx<CompEdges.size() && CompEdges[edgeidx].Ind1<=itnode->first; edgeidx++)
				if(CompNodes[CompEdges[edgeidx].Ind1]==itnode->second && CompNodes[CompEdges[edgeidx].Ind2]==itnode->second+1){
					isfound=true; break;
				}
			if(!isfound){
				std::map<int,int>::iterator tmpit=itnode; tmpit++;
				Edge_t tmp(itnode->first, false, tmpit->first, true, 1);
				CompEdges.push_back(tmp);
			}
		}
		GenerateILP(CompNodes, CompEdges, env, model, vGRBVar);
		model.optimize();
		vector< vector<int> > Z; Z.resize(CompNodes.size());
		int count=0;
		for(int j=0; j<Z.size(); j++){
			Z[j].resize(CompNodes.size(), 0);
			for(int k=j+1; k<CompNodes.size(); k++){
				Z[j][k]=(int)vGRBVar[(int)CompNodes.size()+(int)CompEdges.size()+count].get(GRB_DoubleAttr_X);
				count++;
			}
		}
		for(int j=0; j<Z.size(); j++)
			for(int k=0; k<j; k++)
				Z[j][k]=1-Z[k][j];
		for(std::map<int,int>::iterator it=CompNodes.begin(); it!=CompNodes.end(); it++){
			int pos=CompNodes.size();
			for(int k=0; k<Z.size(); k++)
				pos-=Z[it->second][k];
			BestOrder[pos-1]=(vGRBVar[it->second].get(GRB_DoubleAttr_X)>0.5)?(it->first+1):(-it->first-1);
		}
		return BestOrder;
	}
	else{
		MinCutEdge_t edges[CompEdges.size()];
		for(int j=0; j<CompEdges.size(); j++){
			edges[j].first=CompNodes[CompEdges[j].Ind1]; edges[j].second=CompNodes[CompEdges[j].Ind2];
		}
		weight_type ws[CompEdges.size()];
		for(int i=0; i<CompEdges.size(); i++)
			ws[i]=1;
		undirected_graph g(edges, edges+CompEdges.size(), ws, CompNodes.size(), CompEdges.size());
		BOOST_AUTO(parities, boost::make_one_bit_color_map(num_vertices(g), get(boost::vertex_index, g)));
		int w = boost::stoer_wagner_min_cut(g, get(boost::edge_weight, g), boost::parity_map(parities));
		if(w>1){
			vector<int> BestOrder(CompNodes.size(), 0);
			GRBEnv env=GRBEnv();;
			GRBModel model=GRBModel(env);
			model.getEnv().set(GRB_IntParam_LogToConsole, 0);
			model.getEnv().set(GRB_DoubleParam_TimeLimit, 300.0);
			vector<GRBVar> vGRBVar;
			int edgeidx=0;
			std::map<int,int>::iterator itnodeend=CompNodes.end(); itnodeend--;
			for(std::map<int,int>::iterator itnode=CompNodes.begin(); itnode!=itnodeend; itnode++){
				bool isfound=false;
				for(; edgeidx<CompEdges.size() && CompEdges[edgeidx].Ind1<=itnode->first; edgeidx++)
					if(CompNodes[CompEdges[edgeidx].Ind1]==itnode->second && CompNodes[CompEdges[edgeidx].Ind2]==itnode->second+1){
						isfound=true; break;
					}
				if(!isfound){
					std::map<int,int>::iterator tmpit=itnode; tmpit++;
					Edge_t tmp(itnode->first, false, tmpit->first, true, 1);
					CompEdges.push_back(tmp);
				}
			}
			GenerateILP(CompNodes, CompEdges, env, model, vGRBVar);
			model.optimize();
			vector< vector<int> > Z; Z.resize(CompNodes.size());
			int count=0;
			for(int j=0; j<Z.size(); j++){
				Z[j].resize(CompNodes.size(), 0);
				for(int k=j+1; k<CompNodes.size(); k++){
					Z[j][k]=(int)vGRBVar[(int)CompNodes.size()+(int)CompEdges.size()+count].get(GRB_DoubleAttr_X);
					count++;
				}
			}
			for(int j=0; j<Z.size(); j++)
				for(int k=0; k<j; k++)
					Z[j][k]=1-Z[k][j];
			for(std::map<int,int>::iterator it=CompNodes.begin(); it!=CompNodes.end(); it++){
				int pos=CompNodes.size();
				for(int k=0; k<Z.size(); k++)
					pos-=Z[it->second][k];
				BestOrder[pos-1]=(vGRBVar[it->second].get(GRB_DoubleAttr_X)>0.5)?(it->first+1):(-it->first-1);
			}
			return BestOrder;
		}
		else{
			std::map<int,int> VertexParty1, VertexParty2;
			int count1=0, count2=0;
			for(std::map<int,int>::iterator it=CompNodes.begin(); it!=CompNodes.end(); it++){
				if (get(parities, it->second))
					VertexParty1[it->first]=count1++;
				else
					VertexParty2[it->first]=count2++;
			}
			vector<Edge_t> EdgesParty1, EdgesParty2;
			Edge_t EdgeMiddle;
			for(vector<Edge_t>::iterator it=CompEdges.begin(); it!=CompEdges.end(); it++){
				if(get(parities, CompNodes[it->Ind1]) && get(parities, CompNodes[it->Ind2]))
					EdgesParty1.push_back(*it);
				else if(!get(parities, CompNodes[it->Ind1]) && !get(parities, CompNodes[it->Ind2]))
					EdgesParty2.push_back(*it);
				else
					EdgeMiddle=(*it);
			}
			vector<int> BestOrder;
			vector<int> BestParty1=MincutRecursion(VertexParty1, EdgesParty1);
			vector<int> BestParty2=MincutRecursion(VertexParty2, EdgesParty2);
			// decide which party goes first by median
			int median1=0, median2=0;
			bool ispositive1=false, ishead1=false, ispositive2=false, ishead2=false;
			vector<int> tmp;
			for(int i=0; i<BestParty1.size(); i++){
				tmp.push_back(abs(BestParty1[i]));
				if(abs(BestParty1[i])==EdgeMiddle.Ind1+1){
					ispositive1=(BestParty1[i]>0); ishead1=EdgeMiddle.Head1;
				}
				else if(abs(BestParty1[i])==EdgeMiddle.Ind2+1){
					ispositive1=(BestParty1[i]>0); ishead1=EdgeMiddle.Head2;
				}
			}
			sort(tmp.begin(), tmp.end());
			median1=tmp[((int)tmp.size()-1)/2];
			tmp.clear();
			for(int i=0; i<BestParty2.size(); i++){
				tmp.push_back(abs(BestParty2[i]));
				if(abs(BestParty2[i])==EdgeMiddle.Ind1+1){
					ispositive2=(BestParty2[i]>0); ishead2=EdgeMiddle.Head1;
				}
				else if(abs(BestParty2[i])==EdgeMiddle.Ind2+1){
					ispositive2=(BestParty2[i]>0); ishead2=EdgeMiddle.Head2;
				}
			}
			sort(tmp.begin(), tmp.end());
			median2=tmp[((int)tmp.size()-1)/2];
			if(median1<median2){
				if((ispositive1 && ishead1) || (!ispositive1 && !ishead1)){
					reverse(BestParty1.begin(), BestParty1.end());
					for(int i=0; i<BestParty1.size(); i++)
						BestParty1[i]=-BestParty1[i];
				}
				if((ispositive2 && !ishead2) || (!ispositive2 && ishead2)){
					reverse(BestParty2.begin(), BestParty2.end());
					for(int i=0; i<BestParty2.size(); i++)
						BestParty2[i]=-BestParty2[i];
				}
				BestOrder=BestParty1;
				BestOrder.insert(BestOrder.end(), BestParty2.begin(), BestParty2.end());
			}
			else{
				if((ispositive2 && ishead2) || (!ispositive2 && !ishead2)){
					reverse(BestParty2.begin(), BestParty2.end());
					for(int i=0; i<BestParty2.size(); i++)
						BestParty2[i]=-BestParty2[i];
				}
				if((ispositive1 && !ishead1) || (!ispositive1 && ishead1)){
					reverse(BestParty1.begin(), BestParty1.end());
					for(int i=0; i<BestParty1.size(); i++)
						BestParty1[i]=-BestParty1[i];
				}
				BestOrder=BestParty2;
				BestOrder.insert(BestOrder.end(), BestParty1.begin(), BestParty1.end());
			}
			return BestOrder;
		}
	}
};

void SegmentGraph_t::GenerateILP(std::map<int,int>& CompNodes, vector<Edge_t>& CompEdges, GRBEnv& env, GRBModel& model, vector<GRBVar>& vGRBVar){
	vector<string> nodenames, edgenames, pairnodenames;
	int nodeoffset=0, edgeoffset=CompNodes.size(), pairoffset=CompNodes.size()+CompEdges.size();
	for(int i=0; i<CompNodes.size(); i++){
		nodenames.push_back("y"+to_string(i));
		for(int j=i+1; j<CompNodes.size(); j++)
			pairnodenames.push_back("z"+to_string(i)+to_string(j));
	}
	for(int i=0; i<CompEdges.size(); i++)
		edgenames.push_back("x"+to_string(i));
	// In all variables, node first, edge second, pairorder last
	for(int i=0; i<nodenames.size(); i++){
		GRBVar tmp=model.addVar(0.0, 1.0, 0.0, GRB_BINARY, nodenames[i]);
		vGRBVar.push_back(tmp);
	}
	for(int i=0; i<edgenames.size(); i++){
		GRBVar tmp=model.addVar(0.0, 1.0, CompEdges[i].Weight, GRB_BINARY, edgenames[i]);
		vGRBVar.push_back(tmp);
	}
	int count=0, previndex=0;
	for(int i=0; i<pairnodenames.size(); i++){
		GRBVar tmp=model.addVar(0.0, 1.0, 0.0, GRB_BINARY, pairnodenames[i]);
		vGRBVar.push_back(tmp);
	}
	model.update();
	model.set(GRB_IntAttr_ModelSense, GRB_MAXIMIZE);
	int numconstr=0;
	for(int i=0; i<CompEdges.size(); i++){
		int pairind=0; // index of corresponding pairnodenames in variable vector
		for(int j=0; j<min(CompNodes[CompEdges[i].Ind1], CompNodes[CompEdges[i].Ind2]); j++)
			pairind+=(int)CompNodes.size()-j-1;
		pairind+=abs(CompNodes[CompEdges[i].Ind1]-CompNodes[CompEdges[i].Ind2])-1;
		if((CompNodes[CompEdges[i].Ind1]<CompNodes[CompEdges[i].Ind2] && CompEdges[i].Head1==false && CompEdges[i].Head2==true) || (CompNodes[CompEdges[i].Ind1]>CompNodes[CompEdges[i].Ind2] && CompEdges[i].Head1==true && CompEdges[i].Head2==false)){
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[CompNodes[CompEdges[i].Ind2]] + 1, "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind2]] - vGRBVar[CompNodes[CompEdges[i].Ind1]] + 1, "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[pairoffset+pairind] + 1, "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind1]] + 1, "c"+to_string(numconstr++));
		}
		else if(CompEdges[i].Head1==false && CompEdges[i].Head2==false){
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] + vGRBVar[CompNodes[CompEdges[i].Ind2]], "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= 2 - vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[CompNodes[CompEdges[i].Ind2]], "c"+to_string(numconstr++));
			if(CompNodes[CompEdges[i].Ind1]<CompNodes[CompEdges[i].Ind2]){
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[pairoffset+pairind] + 1, "c"+to_string(numconstr++));
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind1]] + 1, "c"+to_string(numconstr++));
			}
			else{
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind2]] - vGRBVar[pairoffset+pairind] + 1, "c"+to_string(numconstr++));
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind2]] + 1, "c"+to_string(numconstr++));
			}
		}
		else if(CompEdges[i].Head1==true && CompEdges[i].Head2==true){
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] + vGRBVar[CompNodes[CompEdges[i].Ind2]], "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= 2 - vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[CompNodes[CompEdges[i].Ind2]], "c"+to_string(numconstr++));
			if(CompNodes[CompEdges[i].Ind1]<CompNodes[CompEdges[i].Ind2]){
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind2]] - vGRBVar[pairoffset+pairind] + 1, "c"+to_string(numconstr++));
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind2]] + 1, "c"+to_string(numconstr++));
			}
			else{
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[pairoffset+pairind] + 1, "c"+to_string(numconstr++));
				model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind1]] + 1, "c"+to_string(numconstr++));
			}
		}
		else{
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] - vGRBVar[CompNodes[CompEdges[i].Ind2]] + 1, "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind2]] - vGRBVar[CompNodes[CompEdges[i].Ind1]] + 1, "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= vGRBVar[CompNodes[CompEdges[i].Ind1]] + vGRBVar[pairoffset+pairind], "c"+to_string(numconstr++));
			model.addConstr(vGRBVar[edgeoffset+i] <= 2 - vGRBVar[pairoffset+pairind] - vGRBVar[CompNodes[CompEdges[i].Ind1]], "c"+to_string(numconstr++));
		}
	}
	for(int i=0; i<CompNodes.size(); i++){
		for(int j=i+1; j<CompNodes.size(); j++){
			for(int k=j+1; k<CompNodes.size(); k++){
				int pij=pairoffset, pjk=pairoffset, pik=pairoffset;
				for(int l=0; l<i; l++)
					pij+=(int)CompNodes.size()-l-1;
				pij+=(j-i-1);
				for(int l=0; l<j; l++)
					pjk+=(int)CompNodes.size()-l-1;
				pjk+=(k-j-1);
				for(int l=0; l<i; l++)
					pik+=(int)CompNodes.size()-l-1;
				pik+=(k-i-1);
				model.addConstr(vGRBVar[pij] + vGRBVar[pjk] + 1 - vGRBVar[pik] >= 1, "c"+to_string(numconstr++));
				model.addConstr(vGRBVar[pij] + vGRBVar[pjk] + 1 - vGRBVar[pik] <= 2, "c"+to_string(numconstr++));
			}
		}
	}
};

vector< vector<int> > SegmentGraph_t::SortComponents(vector< vector<int> >& Components){
	std::map<int,int> Median_ID;
	vector<int> Median(Components.size(), 0);
	for(int i=0; i<Components.size(); i++){
		vector<int> tmp=Components[i];
		for(int j=0; j<tmp.size(); j++)
			if(tmp[j]<0)
				tmp[j]=-tmp[j];
		sort(tmp.begin(), tmp.end());
		Median[i]=tmp[(tmp.size()-1)/2];
		Median_ID[Median[i]]=i;
	}
	sort(Median.begin(), Median.end());
	vector< vector<int> > NewComponents; NewComponents.resize(Components.size());
	for(int i=0; i<Median.size(); i++){
		NewComponents[i]=Components[Median_ID[Median[i]]];
		// try to make index from small to large, preserve the original sequence
		if(NewComponents[i].size()==1 && NewComponents[i][0]<0)
			NewComponents[i][0]=-NewComponents[i][0];
		int count=0;
		for(int j=0; j<NewComponents[i].size()-1; j++)
			if(abs(NewComponents[i][j])>abs(NewComponents[i][j+1])){
				count++;
			}
		if(count>(int)NewComponents[i].size()/2 || (count==(int)NewComponents[i].size()/2 && abs(NewComponents[i].front())>abs(NewComponents[i].back()))){
			for(int j=0; j<NewComponents[i].size(); j++)
				NewComponents[i][j]=-NewComponents[i][j];
			reverse(NewComponents[i].begin(), NewComponents[i].end());
		}
	}
	return NewComponents;
};

vector< vector<int> > SegmentGraph_t::MergeSingleton(vector< vector<int> >& Components, const vector<int>& RefLength, int LenCutOff){
	vector< vector<int> > NewComponents, Consecutive; NewComponents.reserve(Components.size());
	vector<int> SingletonComponent, tmp;
	for(int i=0; i<Components.size(); i++)
		if(Components[i].size()!=1){
			bool isconsecutive=true;
			for(int j=0; j<Components[i].size()-1; j++)
				if(Components[i][j+1]-Components[i][j]!=1 || vNodes[abs(Components[i][j+1])-1].Chr!=vNodes[abs(Components[i][j])-1].Chr){
					isconsecutive=false; break;
				}
			if(isconsecutive && vNodes[abs(Components[i].front())-1].Position==0 && vNodes[abs(Components[i].back())-1].Position+vNodes[abs(Components[i].back())-1].Length==RefLength[vNodes[abs(Components[i].front())-1].Chr])
				isconsecutive=false;
			if(!isconsecutive)
				NewComponents.push_back(Components[i]);
			else
				Consecutive.push_back(Components[i]);
		}
	int idxconsecutive=0;
	for(int i=0; i<Components.size(); i++){
		if(Components[i].size()==1 && !(vNodes[Components[i][0]-1].Position==0 && vNodes[Components[i][0]-1].Length==RefLength[vNodes[Components[i][0]-1].Chr])){
			if(tmp.size()==0 || (tmp.back()+1==Components[i][0] && vNodes[tmp.back()-1].Chr==vNodes[abs(Components[i][0])-1].Chr))
				tmp.push_back(abs(Components[i][0]));
			else if(tmp.size()==1){
				for(; idxconsecutive<Consecutive.size() && Consecutive[idxconsecutive].back()+1<=tmp[0]; idxconsecutive++)
					if(Consecutive[idxconsecutive].back()+1>=tmp[0] && vNodes[Consecutive[idxconsecutive][(Consecutive[idxconsecutive].size()-1)/2]-1].Chr==vNodes[tmp[0]-1].Chr)
						break;
				int medianidx=(Consecutive.size()!=0)?(Consecutive[idxconsecutive].size()-1)/2:-1;
				if(Consecutive.size()!=0 && idxconsecutive<Consecutive.size() && tmp[0]==Consecutive[idxconsecutive].front()-1 && vNodes[tmp[0]-1].Chr==vNodes[Consecutive[idxconsecutive][medianidx]-1].Chr)
					Consecutive[idxconsecutive].insert(Consecutive[idxconsecutive].begin(), tmp[0]);
				else if(Consecutive.size()!=0 && idxconsecutive<Consecutive.size() && tmp[0]==Consecutive[idxconsecutive].back()+1 && vNodes[tmp[0]-1].Chr==vNodes[Consecutive[idxconsecutive][medianidx]-1].Chr)
					Consecutive[idxconsecutive].push_back(tmp[0]);
				else
					SingletonComponent.push_back(tmp[0]);
				tmp.clear(); tmp.push_back(abs(Components[i][0]));
			}
			else{
				for(; idxconsecutive<Consecutive.size() && Consecutive[idxconsecutive].back()+1<=tmp[0]; idxconsecutive++)
					if(Consecutive[idxconsecutive].back()+1>=tmp[0] && vNodes[Consecutive[idxconsecutive][(Consecutive[idxconsecutive].size()-1)/2]-1].Chr==vNodes[tmp[(tmp.size()-1)/2]-1].Chr)
						break;
				int medianidx=(Consecutive.size()!=0)?(Consecutive[idxconsecutive].size()-1)/2:-1;
				if(Consecutive.size()!=0 && idxconsecutive<Consecutive.size() && tmp.back()==Consecutive[idxconsecutive].front()-1 && vNodes[tmp[(tmp.size()-1)/2]-1].Chr==vNodes[Consecutive[idxconsecutive][medianidx]-1].Chr)
					Consecutive[idxconsecutive].insert(Consecutive[idxconsecutive].begin(), tmp.begin(), tmp.end());
				else if(Consecutive.size()!=0 && idxconsecutive<Consecutive.size() && tmp[0]==Consecutive[idxconsecutive].back()+1 && vNodes[tmp[(tmp.size()-1)/2]-1].Chr==vNodes[Consecutive[idxconsecutive][medianidx]-1].Chr)
					Consecutive[idxconsecutive].insert(Consecutive[idxconsecutive].end(), tmp.begin(), tmp.end());
				else
					Consecutive.push_back(tmp);
				tmp.clear(); tmp.push_back(abs(Components[i][0]));
			}
		}
		else if(Components[i].size()==1 && vNodes[Components[i][0]-1].Position==0 && vNodes[Components[i][0]-1].Length==RefLength[vNodes[Components[i][0]-1].Chr])
			NewComponents.push_back(Components[i]);
	}
	if(tmp.size()>1)
		Consecutive.push_back(tmp);
	else if(tmp.size()==1)
		SingletonComponent.push_back(tmp[0]);
	// insert singleton nodes
	MergeSingleton_Insert(SingletonComponent, NewComponents);
	// push back new consecutive nodes after singleton insertion
	vector< vector<int> > tmpConsecutive, tmpNewComponents; tmpConsecutive.reserve(Consecutive.size()); tmpNewComponents.reserve(NewComponents.size());
	idxconsecutive=0;
	for(int i=0; i<NewComponents.size(); i++){
		bool isconsecutive=true;
		for(int j=0; j<NewComponents[i].size()-1; j++)
			if(NewComponents[i][j+1]-NewComponents[i][j]!=1 || vNodes[abs(NewComponents[i][j+1])-1].Chr!=vNodes[abs(NewComponents[i][j])-1].Chr){
				isconsecutive=false; break;
			}
		if(isconsecutive && vNodes[abs(NewComponents[i].front())-1].Position==0 && vNodes[abs(NewComponents[i].back())-1].Position+vNodes[abs(NewComponents[i].back())-1].Length==RefLength[vNodes[abs(NewComponents[i].front())-1].Chr])
			isconsecutive=false;
		if(!isconsecutive || NewComponents[i].size()==1)
			tmpNewComponents.push_back(NewComponents[i]);
		else{
			int lastidx=idxconsecutive;
			for(; idxconsecutive<Consecutive.size() && Consecutive[idxconsecutive].back()<NewComponents[i].front(); idxconsecutive++){}
			for(int j=lastidx; j<idxconsecutive; j++)
				tmpConsecutive.push_back(Consecutive[j]);
			tmpConsecutive.push_back(NewComponents[i]);
		}
	}
	for(int j=idxconsecutive; j<Consecutive.size(); j++)
		tmpConsecutive.push_back(Consecutive[j]);
	Consecutive=tmpConsecutive; tmpConsecutive.clear();
	NewComponents=tmpNewComponents; tmpNewComponents.clear();
	for(int i=0; i<Consecutive.size(); i++){
		if(tmpConsecutive.size()==0 || tmpConsecutive.back().back()+1!=Consecutive[i].front() || vNodes[abs(tmpConsecutive.back().back())-1].Chr!=vNodes[abs(Consecutive[i].back())-1].Chr)
			tmpConsecutive.push_back(Consecutive[i]);
		else
			tmpConsecutive.back().insert(tmpConsecutive.back().end(), Consecutive[i].begin(), Consecutive[i].end());
	}
	tmpConsecutive.reserve(tmpConsecutive.size());
	Consecutive=tmpConsecutive; tmpConsecutive.clear();
	// insert consecutive nodes
	MergeSingleton_Insert(Consecutive, NewComponents);
	return NewComponents;
};

bool SegmentGraph_t::MergeSingleton_Insert(vector<int> SingletonComponent, vector< vector<int> >& NewComponents){
	clock_t starttime=clock();
	double duration;
	vector<int> Median(NewComponents.size(), 0);
	for(int i=0; i<NewComponents.size(); i++){
		vector<int> tmp;
		for(int j=0; j<NewComponents[i].size(); j++)
			tmp.push_back(abs(NewComponents[i][j]));
		sort(tmp.begin(), tmp.end());
		Median[i]=tmp[((int)tmp.size()-1)/2];
	}
	typedef tuple<int,int,bool> InsertionPlace_t;
	vector< vector<InsertionPlace_t> > InsertionComponents;
	InsertionComponents.resize(NewComponents.size());
	vector<int> UnInserted;
	for(int i=0; i<SingletonComponent.size(); i++){
		if(i%5000==0){
			duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
			cout<<i<<'\t'<<"used time "<<duration<<endl;
		}
		int diffmedian1=vNodes.size(), diffmedian2=vNodes.size(), diffadja=50;
		int Idxadja=-1, Idxmedian=-1;
		int eleadja=0, elemedian=0;
		for(int j=0; j<NewComponents.size(); j++){
			// find adjacent
			for(int k=0; k<NewComponents[j].size()-1; k++){ // whether place between k and k+1 is better
				// before small, after large
				int diffsmall=vNodes.size(), difflarge=vNodes.size();
				bool flagsmall, flaglarge;
				for(int l=max(0, k-1); l<=k; l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[abs(SingletonComponent[i])-1].Chr && abs(NewComponents[j][l])<abs(SingletonComponent[i]) && abs(SingletonComponent[i])-abs(NewComponents[j][l])<diffsmall){
						diffsmall=abs(SingletonComponent[i])-abs(NewComponents[j][l]);
						flagsmall=(NewComponents[j][l]<0);
					}
				for(int l=k+1; l<min((int)NewComponents[j].size(), k+3); l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[abs(SingletonComponent[i])-1].Chr && abs(NewComponents[j][l])>abs(SingletonComponent[i]) && abs(NewComponents[j][l])-abs(SingletonComponent[i])<difflarge){
						difflarge=abs(NewComponents[j][l])-abs(SingletonComponent[i]);
						flaglarge=(NewComponents[j][l]<0);
					}
				if(diffsmall+difflarge<abs(diffadja) && !(flagsmall && flaglarge)){
					diffadja=diffsmall+difflarge; Idxadja=j; eleadja=k;
				}
				// before large, after small
				diffsmall=vNodes.size(); difflarge=vNodes.size();
				for(int l=max(0, k-1); l<=k; l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[abs(SingletonComponent[i])-1].Chr && abs(NewComponents[j][l])>abs(SingletonComponent[i]) && abs(NewComponents[j][l])-abs(SingletonComponent[i])<difflarge){
						difflarge=abs(NewComponents[j][l])-abs(SingletonComponent[i]);
						flaglarge=(NewComponents[j][l]>0);
					}
				for(int l=k+1; l<min((int)NewComponents[j].size(), k+3); l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[abs(SingletonComponent[i])-1].Chr && abs(NewComponents[j][l])<abs(SingletonComponent[i]) && abs(SingletonComponent[i])-abs(NewComponents[j][l])<diffsmall){
						diffsmall=abs(SingletonComponent[i])-abs(NewComponents[j][l]);
						flagsmall=(NewComponents[j][l]>0);
					}
				if(diffsmall+difflarge<abs(diffadja) && !(flagsmall && flaglarge)){
					diffadja=-(diffsmall+difflarge); Idxadja=j; eleadja=k;
				}
			}
			// find closest median
			if(vNodes[Median[j]-1].Chr==vNodes[SingletonComponent[i]-1].Chr && abs(Median[j]-abs(SingletonComponent[i]))<diffmedian1){
				for(int k=0; k<NewComponents[j].size(); k++)
					if(abs(abs(NewComponents[j][k])-abs(SingletonComponent[i]))<abs(diffmedian2)){
						diffmedian2=abs(NewComponents[j][k])-abs(SingletonComponent[i]); diffmedian1=abs(Median[j]-abs(SingletonComponent[i]));
						Idxmedian=j; elemedian=k;
					}
			}
		}
		// decide whether median or adjacent
		if((Idxadja==Idxmedian && Idxadja!=-1) || (abs(diffadja)<abs(diffmedian2) && Idxadja!=-1)){
			if(diffadja>0){
				InsertionComponents[Idxadja].push_back(make_tuple(abs(SingletonComponent[i]),eleadja+1, true));
			}
			else{
				InsertionComponents[Idxadja].push_back(make_tuple(abs(SingletonComponent[i]),eleadja+1, false));
			}
		}
		else if(Idxmedian!=-1){
			if(diffmedian2<0){
				InsertionComponents[Idxmedian].push_back(make_tuple(abs(SingletonComponent[i]),NewComponents[Idxmedian].size(), true));
			}
			else if(diffmedian2>0){
				InsertionComponents[Idxmedian].push_back(make_tuple(abs(SingletonComponent[i]), 0, true));
			}
		}
		else
			UnInserted.push_back(abs(SingletonComponent[i]));
	}
	duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
	starttime=clock();
	cout<<"locate all insertion place. "<<duration<<endl;
	// insert by InsertionComponents
	vector< vector<int> > tmpNewComponents; tmpNewComponents.reserve(NewComponents.size());
	for(int i=0; i<InsertionComponents.size(); i++){
		vector<int> tmp;
		sort(InsertionComponents[i].begin(), InsertionComponents[i].end(), [](InsertionPlace_t a, InsertionPlace_t b){if(get<1>(a)!=get<1>(b)) return get<1>(a)<get<1>(b); else return get<0>(a)<get<0>(b);});
		int j=0;
		for(int k=0; k<NewComponents[i].size(); k++){
			if(j>=InsertionComponents[i].size() || k<get<1>(InsertionComponents[i][j]))
				tmp.push_back(NewComponents[i][k]);
			else{
				vector<int> tmp1;
				int count=0;
				for(; j<InsertionComponents[i].size() && get<1>(InsertionComponents[i][j])<=k; j++){
					if(get<2>(InsertionComponents[i][j]))
						tmp1.push_back(get<0>(InsertionComponents[i][j]));
					else{
						tmp1.push_back(-get<0>(InsertionComponents[i][j]));
						count++;
					}
				}
				if(count>tmp1.size()/2)
					reverse(tmp1.begin(), tmp1.end());
				tmp.insert(tmp.end(), tmp1.begin(), tmp1.end());
				tmp.push_back(NewComponents[i][k]);
			}
		}
		if(j<InsertionComponents[i].size()){
			vector<int> tmp1;
			int count=0;
			for(; j<InsertionComponents[i].size(); j++){
				if(get<2>(InsertionComponents[i][j]))
					tmp1.push_back(get<0>(InsertionComponents[i][j]));
				else{
					tmp1.push_back(-get<0>(InsertionComponents[i][j]));
					count++;
				}
			}
			if(count>tmp1.size()/2)
				reverse(tmp1.begin(), tmp1.end());
			tmp.insert(tmp.end(), tmp1.begin(), tmp1.end());
		}
		tmpNewComponents.push_back(tmp);
	}
	duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
	starttime=clock();
	cout<<"finish insertion. "<<duration<<endl;
	NewComponents=tmpNewComponents; tmpNewComponents.clear();
	for(int i=0; i<UnInserted.size(); i++){
		vector<int> tmp; tmp.push_back(abs(UnInserted[i]));
		NewComponents.push_back(tmp);
	}
	return true;
};

bool SegmentGraph_t::MergeSingleton_Insert(vector< vector<int> > Consecutive, vector< vector<int> >& NewComponents){
	clock_t starttime=clock();
	double duration;
	// median of all existing NewComponents
	vector<int> Median(NewComponents.size(), 0);
	for(int i=0; i<NewComponents.size(); i++){
		vector<int> tmp;
		for(int j=0; j<NewComponents[i].size(); j++)
			tmp.push_back(abs(NewComponents[i][j]));
		sort(tmp.begin(), tmp.end());
		Median[i]=tmp[((int)tmp.size()-1)/2];
	}
	// median of all Consecutive
	vector<int> ConsecutiveMedian(Consecutive.size(), 0);
	for(int i=0; i<Consecutive.size(); i++){
		vector<int> tmp;
		for(int j=0; j<Consecutive[i].size(); j++)
			tmp.push_back(abs(Consecutive[i][j]));
		sort(tmp.begin(), tmp.end());
		ConsecutiveMedian[i]=tmp[((int)tmp.size()-1)/2];
	}
	// find insertion position (adjacent or closest median)
	typedef tuple< vector<int> ,int,bool> InsertionPlace_t;
	vector< vector<InsertionPlace_t> > InsertionComponents;
	InsertionComponents.resize(NewComponents.size());
	vector< vector<int> > UnInserted;
	for(int i=0; i<Consecutive.size(); i++){
		if(i%1000==0){
			duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
			cout<<i<<'\t'<<"used time "<<duration<<endl;
		}
		int diffmedian1=vNodes.size(), diffmedian2=vNodes.size(), diffadja=50;
		int Idxadja=-1, Idxmedian=-1;
		int eleadja=0, elemedian=0;
		for(int j=0; j<NewComponents.size(); j++){
			// find adjacent
			for(int k=0; k<NewComponents[j].size()-1; k++){ // whether place between k and k+1 is better
				// before small, after large
				int diffsmall=vNodes.size(), difflarge=vNodes.size();
				bool flagsmall, flaglarge;
				for(int l=max(0, k-1); l<=k; l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[ConsecutiveMedian[i]-1].Chr && abs(NewComponents[j][l])<abs(Consecutive[i][0]) && abs(Consecutive[i][0])-abs(NewComponents[j][l])<diffsmall){
						diffsmall=abs(Consecutive[i][0])-abs(NewComponents[j][l]);
						flagsmall=(NewComponents[j][l]<0);
					}
				for(int l=k+1; l<min((int)NewComponents[j].size(), k+3); l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[ConsecutiveMedian[i]-1].Chr && abs(NewComponents[j][l])>abs(Consecutive[i].back()) && abs(NewComponents[j][l])-abs(Consecutive[i].back())<difflarge){
						difflarge=abs(NewComponents[j][l])-abs(Consecutive[i].back());
						flaglarge=(NewComponents[j][l]<0);
					}
				if(diffsmall+difflarge<abs(diffadja) && !(flagsmall && flaglarge)){
					diffadja=diffsmall+difflarge; Idxadja=j; eleadja=k;
				}
				// before large, after small
				diffsmall=vNodes.size(); difflarge=vNodes.size();
				for(int l=max(0, k-1); l<=k; l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[ConsecutiveMedian[i]-1].Chr && abs(NewComponents[j][l])>abs(Consecutive[i].back()) && abs(NewComponents[j][l])-abs(Consecutive[i].back())<difflarge){
						difflarge=abs(NewComponents[j][l])-abs(Consecutive[i].back());
						flaglarge=(NewComponents[j][l]>0);
					}
				for(int l=k+1; l<min((int)NewComponents[j].size(), k+3); l++)
					if(vNodes[abs(NewComponents[j][l])-1].Chr==vNodes[ConsecutiveMedian[i]-1].Chr && abs(NewComponents[j][l])<abs(Consecutive[i][0]) && abs(Consecutive[i][0])-abs(NewComponents[j][l])<diffsmall){
						diffsmall=abs(Consecutive[i][0])-abs(NewComponents[j][l]);
						flagsmall=(NewComponents[j][l]>0);
					}
				if(diffsmall+difflarge<abs(diffadja) && !(flagsmall && flaglarge)){
					diffadja=-(diffsmall+difflarge); Idxadja=j; eleadja=k;
				}
			}
			// find closest median
			if(vNodes[Median[j]-1].Chr==vNodes[ConsecutiveMedian[i]-1].Chr && abs(Median[j]-ConsecutiveMedian[i])<diffmedian1){
				for(int k=0; k<NewComponents[j].size(); k++)
					if(abs(abs(NewComponents[j][k])-ConsecutiveMedian[i])<abs(diffmedian2)){
						diffmedian2=abs(NewComponents[j][k])-ConsecutiveMedian[i]; diffmedian1=abs(Median[j]-ConsecutiveMedian[i]);
						Idxmedian=j; elemedian=k;
					}
			}
		}
		// decide whether median or adjacent
		if((Idxadja==Idxmedian && Idxadja!=-1) || (abs(diffadja)<abs(diffmedian2) && Idxadja!=-1)){
			if(diffadja>0){
				InsertionComponents[Idxadja].push_back(make_tuple(Consecutive[i],eleadja+1, true));
			}
			else{
				InsertionComponents[Idxadja].push_back(make_tuple(Consecutive[i],eleadja+1, false));
			}
		}
		else if(Idxmedian!=-1){
			if(diffmedian2<0){
				InsertionComponents[Idxmedian].push_back(make_tuple(Consecutive[i],NewComponents[Idxmedian].size(), true));
			}
			else{
				InsertionComponents[Idxmedian].push_back(make_tuple(Consecutive[i], 0, true));
			}
		}
		else
			UnInserted.push_back(Consecutive[i]);
	}
	duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
	starttime=clock();
	cout<<"locate all insertion place. "<<duration<<endl;
	// insert by InsertionComponents
	vector< vector<int> > tmpNewComponents; tmpNewComponents.reserve(NewComponents.size());
	for(int i=0; i<InsertionComponents.size(); i++){
		vector<int> tmp;
		sort(InsertionComponents[i].begin(), InsertionComponents[i].end(), [](InsertionPlace_t a, InsertionPlace_t b){if(get<1>(a)!=get<1>(b)) return get<1>(a)<get<1>(b); else return get<0>(a).front()<get<0>(b).front();});
		int j=0;
		for(int k=0; k<NewComponents[i].size(); k++){
			if(j>=InsertionComponents[i].size() || k<get<1>(InsertionComponents[i][j]))
				tmp.push_back(NewComponents[i][k]);
			else{
				vector<int> tmp1;
				for(; j<InsertionComponents[i].size() && get<1>(InsertionComponents[i][j])<=k; j++){
					vector<int> curConsecutive=get<0>(InsertionComponents[i][j]);
					if(get<2>(InsertionComponents[i][j]))
						tmp1.insert(tmp1.end(), curConsecutive.begin(), curConsecutive.end());
					else{
						vector<int> reverseConsecutive(curConsecutive.size());
						for(int l=0; l<curConsecutive.size(); l++)
							reverseConsecutive[l]=-curConsecutive[(int)curConsecutive.size()-1-l];
						tmp1.insert(tmp1.begin(),reverseConsecutive.begin(), reverseConsecutive.end());
					}
				}
				tmp.insert(tmp.end(), tmp1.begin(), tmp1.end());
				tmp.push_back(NewComponents[i][k]);
			}
		}
		if(j<InsertionComponents[i].size()){
			vector<int> tmp1;
			int count=0;
			for(; j<InsertionComponents[i].size(); j++){
				vector<int> curConsecutive=get<0>(InsertionComponents[i][j]);
				if(get<2>(InsertionComponents[i][j]))
					tmp1.insert(tmp1.end(), curConsecutive.begin(), curConsecutive.end());
				else{
					vector<int> reverseConsecutive(curConsecutive.size());
					for(int l=0; l<curConsecutive.size(); l++)
						reverseConsecutive[l]=-curConsecutive[(int)curConsecutive.size()-1-l];
					tmp1.insert(tmp1.begin(),reverseConsecutive.begin(), reverseConsecutive.end());
				}
			}
			tmp.insert(tmp.end(), tmp1.begin(), tmp1.end());
		}
		tmpNewComponents.push_back(tmp);
	}
	duration=1.0*(clock()-starttime)/CLOCKS_PER_SEC;
	starttime=clock();
	cout<<"finish insertion. "<<duration<<endl;
	NewComponents=tmpNewComponents; tmpNewComponents.clear();
	for(int i=0; i<UnInserted.size(); i++)
		NewComponents.push_back(UnInserted[i]);
	return true;
};

vector< vector<int> > SegmentGraph_t::MergeComponents(vector< vector<int> >& Components, int LenCutOff){
	vector<int> ChromoMargin;
	for(int i=0; i<vNodes.size()-1; i++)
		if(vNodes[i].Chr!=vNodes[i+1].Chr)
			ChromoMargin.push_back(i+1);
	vector< vector<int> > NewComponents; NewComponents.reserve(Components.size());
	for(int i=0; i<Components.size(); i++){
		if(NewComponents.size()==0)
			NewComponents.push_back(Components[i]);
		else{
			// calculate length and median of this component
			int curLen=0, curMedian;
			vector<int> tmp=Components[i];
			for(int k=0; k<tmp.size(); k++){
				curLen+=vNodes[abs(tmp[k])-1].Length;
				if(tmp[k]<0)
					tmp[k]=-tmp[k];
			}
			sort(tmp.begin(), tmp.end());
			curMedian=tmp[(tmp.size()-1)/2];
			vector<int> reversecomponent;
			for(int k=0; k<Components[i].size(); k++)
				reversecomponent.push_back(-Components[i][Components[i].size()-1-k]);
			// calculate median of pushed back components
			vector<int> Median(NewComponents.size(), 0);
			for(int j=0; j<NewComponents.size(); j++){
				vector<int> tmp=NewComponents[j];
				for(int k=0; k<tmp.size(); k++)
					if(tmp[k]<0)
						tmp[k]=-tmp[k];
				sort(tmp.begin(), tmp.end());
				Median[j]=tmp[(tmp.size()-1)/2];
			}
			// choose the closest NewComponents median, and concatenate current Components[i] to it if they are on the same Chr
			vector<int>::iterator iteleplus, iteleminus;
			int plusidx=NewComponents.size(), minusidx=NewComponents.size();
			int ind=0, diff=abs(curMedian-Median[0])+1;
			for(int j=0; j<Median.size(); j++)
				if(abs(Median[j]-curMedian)<diff){
					for(vector<int>::iterator itele=NewComponents[j].begin(); itele!=NewComponents[j].end(); itele++){
						if(abs(*itele)==abs(Components[i].front())-1){
							iteleminus=itele; minusidx=j;
						}
						else if(abs(*itele)==abs(Components[j].back())+1){
							iteleplus=itele; plusidx=j;
						}
					}
					diff=abs(Median[j]-curMedian);
					ind=j;
				}
			int j;
			for(j=0; j<ChromoMargin.size(); j++)
				if((Median[ind]<=ChromoMargin[j] && curMedian>ChromoMargin[j]) || (Median[ind]>ChromoMargin[j] && curMedian<=ChromoMargin[j]))
					break;
			if(j!=ChromoMargin.size())
				NewComponents.push_back(Components[i]);
			else if(curLen<LenCutOff && plusidx!=NewComponents.size() && minusidx!=NewComponents.size() && plusidx==minusidx && distance(iteleplus, iteleminus)==1 && !(*iteleplus>0 && *iteleminus>0))
				NewComponents[minusidx].insert(iteleminus, reversecomponent.begin(), reversecomponent.end());
			else if(curLen<LenCutOff && plusidx!=NewComponents.size() && minusidx!=NewComponents.size() && plusidx==minusidx && distance(iteleplus, iteleminus)==-1 && !(*iteleplus<0 && *iteleminus<0))
				NewComponents[plusidx].insert(iteleplus, Components[i].begin(), Components[i].end());
			else{
				NewComponents[ind].insert(NewComponents[ind].end(), Components[i].begin(), Components[i].end());
			}
		}
	}
	NewComponents.reserve(NewComponents.size());
	return NewComponents;
};
