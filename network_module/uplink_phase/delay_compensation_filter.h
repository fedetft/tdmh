#ifndef DELAY_COMPENSATION_FILTER_H_
#define DELAY_COMPENSATION_FILTER_H_

namespace mxnet
{

class DelayCompensationFilter
{
	public:
		DelayCompensationFilter()	{};

		void addValue(int newVal)
		{
			if(first)
            {
                first = false;
                filteredVal = newVal;
            } 
            else 
            {
                filteredVal = k*filteredVal + (1.0f-k)*newVal;
            }
		}

		int  getFilteredValue()
		{
			return filteredVal;
		}

		bool hasValue()
		{
			return !first;
		}

	private:
		int 			filteredVal=0;
		bool 			first = true;
		const float 	k = 0.75f;
};

}

#endif
