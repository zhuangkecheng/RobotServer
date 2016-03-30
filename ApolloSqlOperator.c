


#include "ApolloSqlOperator.h"
#include "ApolloUtil.h"
#include "ApolloRedis.h"
#include "ApolloProtocol.h"

MySqlConnectionPool *getMySqlConnectionPool_Instance(void) {
	if (ConnectionPool_Instance == NULL) {
		ConnectionPool_Instance = (MySqlConnectionPool*)malloc(sizeof(MySqlConnectionPool));
		if (ConnectionPool_Instance == NULL) return ConnectionPool_Instance;
		memset(ConnectionPool_Instance, 0, sizeof(MySqlConnectionPool));
	}
	return ConnectionPool_Instance;
}

char init_mysql_pool(void) {
	int i;
	MySqlConnectionPool *instance = getMySqlConnectionPool_Instance();

	pthread_mutex_init(&instance->PoolLock, NULL);
	for (i = 0;i < CONNECTION_POOL_SIZE;i ++) {
		instance->ConnQueue[i].Conn = mysql_init(NULL);
		if (instance->ConnQueue[i].Conn == NULL) {
			continue;
		} else {
			instance->ConnQueue[i].Conn = mysql_real_connect(instance->ConnQueue[i].Conn, MYSQL_HOST, MYSQL_USER, MYSQL_PWD, MYSQL_DB, 0, NULL, 0);
			if (instance->ConnQueue[i].Conn) {
				instance->ConnQueue[i].Status = CONNECT_OFF;
			} else {
				mysql_close(instance->ConnQueue[i].Conn);
				instance->ConnQueue[i].Status = CONNECT_CLOSE;
			}
		}
	}
	return OK;
}

MYSQL* get_connection_from_pool(void) {
	int i;	
	MySqlConnectionPool *instance = getMySqlConnectionPool_Instance();

	pthread_mutex_lock(&instance->PoolLock);
	for (i = 0;i < CONNECTION_POOL_SIZE;i ++) {
		if (instance->ConnQueue[i].Status== CONNECT_OFF) { //get connection
			instance->ConnQueue[i].Status = CONNECT_ON;
			pthread_mutex_unlock(&instance->PoolLock);
			return instance->ConnQueue[i].Conn;
		} else if (instance->ConnQueue[i].Status== CONNECT_CLOSE) { //create connection that init failed
			instance->ConnQueue[i].Conn = mysql_init(NULL);
			if (instance->ConnQueue[i].Conn == NULL) {
				continue;
			}  else {
				instance->ConnQueue[i].Conn = mysql_real_connect(instance->ConnQueue[i].Conn, MYSQL_HOST, MYSQL_USER, MYSQL_PWD, MYSQL_DB, 0, NULL, 0);
				if (instance->ConnQueue[i].Conn) {
					instance->ConnQueue[i].Status = CONNECT_OFF;
				} else {
					mysql_close(instance->ConnQueue[i].Conn);
					instance->ConnQueue[i].Status = CONNECT_CLOSE;
				}
			}
		} else if (instance->ConnQueue[i].Status = CONNECT_ON) { 
			continue;
		}
	}
	pthread_mutex_unlock(&instance->PoolLock);
	return NULL;
}

char return_connection_to_pool(MYSQL *Conn) {
	int i;	
	MySqlConnectionPool *instance = getMySqlConnectionPool_Instance();
	
	pthread_mutex_lock(&instance->PoolLock);
	for (i = 0;i < CONNECTION_POOL_SIZE;i ++) {
		if (Conn->thread_id != instance->ConnQueue[i].Conn->thread_id) {
			continue;
		} else {
			instance->ConnQueue[i].Status = CONNECT_OFF;
			break;
		}
	}
	if (i == CONNECTION_POOL_SIZE) {
		apollo_printf("Connect id:%ld is error, don't exist\n", Conn->thread_id);
		return NOEXIST;
	}
	pthread_mutex_unlock(&instance->PoolLock);
	return OK;
}



/* ** **** ********  ****************  Sql Operation  ****************  ******** **** ** */

int insertDeviceGps(const char* longitude, const char* latitude, const char* high, const char* device_id) {
	MYSQL *Conn;
	int res = -1;
	char sql[SQL_LANGUAGE_LENGTH] = {0};

	sprintf(sql, SQL_INSERT_DEVICE_GPS, longitude, latitude, high, device_id);
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, sql);
	if (res) {
		apollo_printf("INSERT error: %s\n", mysql_error(Conn));
		return NOINSERT;
	} 
	
	apollo_printf("INSERT OK");
	return OK;
}


int insertUser(const char* userName, const char* password) {
	MYSQL *Conn;
	int res = -1;
	char sql[SQL_LANGUAGE_LENGTH] = {0};

	sprintf(sql, SQL_INSERT_USER, userName, password);
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, sql);
	if (res) {
		apollo_printf("INSERT error: %s\n", mysql_error(Conn));
		return NOINSERT;
	} 
	
	apollo_printf("INSERT OK");
	return OK;
}


int selectUserId(const char* userName, const char* password, unsigned int *userId) {
	MYSQL *Conn;
	MYSQL_RES *res_ptr;
	MYSQL_ROW sqlrow;
	int res = -1;
	char sql[SQL_LANGUAGE_LENGTH] = {0};

	sprintf(sql, SQL_SELECT_USERID, userName, password);
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, sql);
	if (!res) {
		if ((res_ptr = mysql_store_result(Conn)) == NULL) {
			apollo_printf("mysql_store_result() Error, %s\n", mysql_error(Conn));  
			return NOEXIST;
		}
		while (sqlrow = mysql_fetch_row(res_ptr)) {
			//value = get_value_fromjson(APOLLO_JSON_KEY, (char*)sqlrow[0]);
			apollo_printf("sqlrow0:%s\n", (char*)sqlrow[0]);
			//sprintf(UUserName, "U_%s", (char*)sqlrow[1]);
			*userId = atoi((char*)sqlrow[0]);
		}
	} 
	
	apollo_printf("INSERT OK");
	return OK;
}

int updatePassword(const char* userName, const char* password) {
	MYSQL *Conn;
	int res = -1;
	char sql[SQL_LANGUAGE_LENGTH] = {0};

	sprintf(sql, SQL_SELECT_UPDATE_PASSWORD, password, userName);
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, sql);
	if (!res) {
		apollo_printf("Update error: %s\n", mysql_error(Conn));
		return NOUPDATE;
	} 
	
	apollo_printf("Update OK");
	return OK;
}


int selectLocationNewItem(char *u8DeviceId, int UserId) {
	MYSQL *Conn;
	MYSQL_RES *res_ptr;
	MYSQL_ROW sqlrow;
	int res = -1;
	char sql[SQL_LANGUAGE_LENGTH] = {0};
	char u8Longitude[2*LEN_LONGITUDE_DATA+2] = {0};
	char u8Latitude[2*LEN_LATITUDE_DATA+2] = {0};
	char u8RedisDeviceGpsInfo[REDIS_COMMAND_LENGTH] = {0};
	char u8High[12] = {0};
	char u8UserId[8] = {0};

	sprintf(sql, SQL_SELECT_LOCATION_NEWINFO, u8DeviceId);
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, sql);
	//apollo_printf("res:%d, sql:%s\n",res,sql);
	if (res == 0) {
		if ((res_ptr = mysql_store_result(Conn)) == NULL) {
			apollo_printf("mysql_store_result() Error, %s\n", mysql_error(Conn));  
			return NOEXIST;
		}
		sqlrow = mysql_fetch_row(res_ptr);
		if (!sqlrow) return NOEXIST;
			//value = get_value_fromjson(APOLLO_JSON_KEY, (char*)sqlrow[0]);
		apollo_printf("sqlrow0:%s\n", (char*)sqlrow[0]);
			//sprintf(UUserName, "U_%s", (char*)sqlrow[1]);
			//*userId = atoi((char*)sqlrow[0]);
			strcpy(u8Longitude, (char*)sqlrow[0]);
			strcpy(u8Latitude, (char*)sqlrow[1]);
			strcpy(u8High, (char*)sqlrow[2]);
		//apollo_printf("sqlrow0:%s\n", (char*)sqlrow[2]);
		#if 1
		if (u8High[0] == 0x30) {
			sprintf(u8RedisDeviceGpsInfo, "G_%s_%s_%s_1_12345", u8Longitude, u8Latitude, 
				u8High);
		} else {
			sprintf(u8RedisDeviceGpsInfo, "G_%s_%s_%s_0_12345", u8Longitude, u8Latitude, 
				u8High);
		}
		sprintf(u8UserId, "%d", UserId);
		if (-1 == set_key_toredis(u8UserId, u8RedisDeviceGpsInfo)) {
			return -2;
		}
		#endif
		apollo_printf("lng:%s, lat:%s, high:%s", u8Longitude, u8Latitude, u8High);
	} 

	//apollo_printf("lng:%s, lat:%s, high:%s", (char*)sqlrow[0], (char*)sqlrow[1], (char*)sqlrow[2]);
	
	//apollo_printf("INSERT OK");
	return OK;
}



void initUserInfoList(void) {
	MYSQL* Conn = NULL;
	MYSQL_RES *res_ptr;
	MYSQL_ROW sqlrow;

	char UUserName[20] = {0};

	int res = -1;	
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, SQL_SELECT_USERINFO_LIST);
	if (res) {
		apollo_printf("SELECT error: %s\n", mysql_error(Conn));
	} else {
		if ((res_ptr = mysql_store_result(Conn)) == NULL) {
			apollo_printf("mysql_store_result() Error, %s\n", mysql_error(Conn));  
			return ;
		}
		while (sqlrow = mysql_fetch_row(res_ptr)) {
			//value = get_value_fromjson(APOLLO_JSON_KEY, (char*)sqlrow[0]);
			apollo_printf("sqlrow0:%s,sqlrow1:%s\n", (char*)sqlrow[0], (char*)sqlrow[1]);
			sprintf(UUserName, "U_%s", (char*)sqlrow[1]);
			if (-1 == set_key_toredis(UUserName, (char*)sqlrow[0])) {
				//return -2;
				return ;
			}
		}
	}

	char r = return_connection_to_pool(Conn);

	return ;
}


void initUserAndDeviceRelationshipList(void) {
	MYSQL* Conn = NULL;
	MYSQL_RES *res_ptr;
	MYSQL_ROW sqlrow;

	char RPUserId[20] = {0}, RDeviceId[20] = {0};

	int res = -1;	
	do {
		Conn = get_connection_from_pool();
		if (Conn != NULL) {
			break;
		} 
		sleep(1);
	} while (1);

	res = mysql_query(Conn, SQL_SELECT_RELATIONSHIP_LIST);
	if (res) {
		apollo_printf("SELECT error: %s\n", mysql_error(Conn));
	} else {
		if ((res_ptr = mysql_store_result(Conn)) == NULL) {
			apollo_printf("mysql_store_result() Error, %s\n", mysql_error(Conn));  
			return ;
		}
		while ((sqlrow = mysql_fetch_row(res_ptr))) {
			//value = get_value_fromjson(APOLLO_JSON_KEY, (char*)sqlrow[0]);
			apollo_printf("sqlrow0:%s,sqlrow1:%s\n", (char*)sqlrow[0], (char*)sqlrow[1]);
			sprintf(RPUserId, "RP_%s", (char*)sqlrow[0]);
			if (-1 == set_key_toredis(RPUserId, (char*)sqlrow[1])) {
				//return -2;
				return ;
			}

			sprintf(RDeviceId, "R_%s", (char*)sqlrow[1]);
			if (-1 == set_key_toredis(RDeviceId, (char*)sqlrow[0])) {
				//return -2;
				return ;
			}
		}
	}

	char r = return_connection_to_pool(Conn);

	return ;
}





