#pragma once
#include "n_common.h"

namespace BAO_Net 
{

	// forward declaration
	template<typename T>
	class connection;

	template<typename T>
	struct msg_header 
	{
		T id{};
		uint32_t size = NULL;
	};

	template<typename T>
	struct message 
	{
		msg_header<T> header {};
		std::vector<uint8_t> body;

		// returns the size of the message in bytes
		size_t getSize() const 
		{
			return sizeof(msg_header<T>) + body.size();
		}

		// cout compatibility
		friend std::ostream& operator << (std::ostream os, const message<T> msg) 
		{
			os << "ID: " << static_cast<int>(msg.header.id) << " Size: " << msg.header.size;
			return os;
		}

		// push POD-Like data into message buffer
		template<typename DT>
		friend message<T>& operator << (message<T>& msg, const DT& data) 
		{
			// TODO: rework in favor of BAOLOGGING
			static_asser(std::is_standard_layout<DT>::value, "FATAL_ERROR: Data is too complicated!");

			size_t tmp = msg.body.size();
			msg.body.resize(msg.body.size() + sizeof(DT));
			std::memcpy(msg.body.data() + tmp, &data, sizeof(DT));
			msg.header.size = msg.getSize();
			
			return msg;
		}

		template<typename DT>
		friend message<T>& operator >> (message<T>& msg, const DT& data)
		{
			// TODO: rework in favor of BAOLOGGING
			static_asser(std::is_standard_layout<DT>::value, "FATAL_ERROR: Data is too complicated!");

			size_t tmp = msg.body.size() - sizeof(DT);
			std::memcpy(&data, msg.body.data() + tmp, sizeof(DT));
			msg.body.resize(tmp);
			msg.header.size = msg.getSize();
			
			return msg;
		}

	};

	template<typename T>
	struct owned_message 
	{
		std::shared_ptr<connection<T>> remote = nullptr;
		message<T> msg;

		friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
		{
			os << msg.msg;
			return os;
		}
	};
}