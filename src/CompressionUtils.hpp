
#ifndef _COMPRESSIONUTILS_HPP_
#define _COMPRESSIONUTILS_HPP_

using namespace std;
class Leon;
//====================================================================================
// ** COMPRESSIONUTILS
//====================================================================================
class CompressionUtils
{
	public:
	
		
		
		//Take a value and return its number of byte
		static int getByteCount(u_int64_t n){
			if(n < 0x100){
				return 1;
			}
			else if(n >= 0x100 && n < 0x10000){
				return 2;
			}
			else if(n >= 0x10000 && n < 0x1000000){
				return 3;
			}
			else if(n >= 0x1000000 && n < 0x100000000){
				return 4;
			}
			else if(n >= 0x100000000 && n < 0x10000000000){
				return 5;
			}
			else if(n >= 0x10000000000 && n < 0x1000000000000){
				return 6;
			}
			else if(n >= 0x1000000000000 && n < 0x100000000000000){
				return 7;
			}
			else{
				return 8;
			}

		}
		
		//Encode a numeric value
		//First encode the byte count of the value
		//Then encode the value of each byte
		static void encodeNumeric(RangeEncoder& rangeEncoder, Order0Model& byteCountModel, vector<Order0Model>& numericModels, u_int64_t value){
			int valueByteCount = getByteCount(value);
			//cout << "Utils: " << (int)valueByteCount << endl;
			rangeEncoder.encode(byteCountModel, valueByteCount);
				
			for(int i=0; i<valueByteCount; i++){
				//cout << "Utils: " << ((value >> i*8) & 0xff) << endl;
				rangeEncoder.encode(numericModels[i], (value >> i*8) & 0xff);
			}
		}

		static void encodeFixedNumeric(RangeEncoder& rangeEncoder, vector<Order0Model>& numericModels, u_int64_t value, int byteCount){
			//cout << "sdf" << value << " " << byteCount << endl;
			for(int i=0; i<byteCount; i++){
				//cout << ((value >> i*8) & 0xff) << endl;
				rangeEncoder.encode(numericModels[i], (value >> i*8) & 0xff);
			}
		}
		
		static u_int64_t decodeNumeric(RangeDecoder& rangeDecoder, Order0Model& byteCountModel, vector<Order0Model>& numericModels){
			u_int8_t byteCount = rangeDecoder.nextByte(byteCountModel);
			//cout << (int)byteCount << endl;
			u_int64_t value = 0;
			for(int i=0; i<byteCount; i++){
				u_int8_t byteValue = rangeDecoder.nextByte(numericModels[i]);
				//cout << "Utils: " << (byteValue << i*8) << endl;
				value |= (byteValue << i*8);
			}
			
			return value;
		}
		
		static u_int64_t decodeFixedNumeric(RangeDecoder& rangeDecoder, vector<Order0Model>& numericModels, int byteCount){
			
			u_int64_t value = 0;
			for(int i=0; i<byteCount; i++){
				
				u_int8_t byteValue = rangeDecoder.nextByte(numericModels[i]);
				value |= (byteValue << i*8);
			}
			
			return value;
		}
		
		/*
		static stringstream _convert;
		
		static string numberToString(u_int64_t number){
			_convert << number;
			string result(_convert.str());
			//cout << result << endl;
			return result;
		}*/
		
		
		static u_int8_t getDeltaValue(u_int64_t value, u_int64_t prevValue, u_int64_t* resultDeltaValue){
			
			bool isDelta1Valid = false;
			bool isDelta2Valid = false;

			u_int64_t deltaValue1 = value - prevValue;
			u_int64_t deltaValue2 = prevValue - value;
			
			
			if(deltaValue1 >= 0 && deltaValue1 < value){
				isDelta1Valid = true;
			}
			if(deltaValue2 >= 0 && deltaValue2 < value){
				isDelta2Valid = true;
			}
			
			if(isDelta1Valid && isDelta2Valid){
				if(deltaValue1 <= deltaValue2){
					*resultDeltaValue = deltaValue1;
					return 1;
				}
				else{
					*resultDeltaValue = deltaValue2;
					return 2;
				}
			}
			else if(isDelta1Valid){
				*resultDeltaValue = deltaValue1;
				return 1;
			}
			else if(isDelta2Valid){
				*resultDeltaValue = deltaValue2;
				return 2;
			}
			
			*resultDeltaValue = value;
			return 0;	
		}
	
	
	
		static u_int64_t getValueFromDelta(u_int8_t deltaType, u_int64_t prevValue, u_int64_t deltaValue){
			
			if(deltaType == 0){
				return deltaValue;
			}
			if(deltaType == 1){
				return prevValue + deltaValue;	
			}
			else if(deltaType == 2){
				return prevValue - deltaValue;
			}
		}
};

	

	

#endif /* _COMPRESSIONUTILS_HPP_ */

