#ifndef MITK_BASE_XML_WRITER
#define MITK_BASE_XML_WRITER

#include <fstream>
#include <string>
#include <stack>
#include <sstream>

namespace mitk{

	class BaseXMLWriter {

    class StreamNode
    {
    public:
      StreamNode( std::string name ): m_Name(name){};
      std::string GetName() { return m_Name; };
      std::ostream& GetPropertyStream() { return m_PropertyStream; };
      std::ostream& GetChildrenStream() { return m_ChildrenStream; };
      std::ostream& GetComment() { return m_Comment; };      

      void Write( std::ostream& out, int steps );
    private:
      std::string m_Name;
      std::stringstream m_PropertyStream;
      std::stringstream m_ChildrenStream;
      std::stringstream m_Comment;
    };

    std::stack<StreamNode*> m_Stack;
		std::ostream* m_Out;
		int m_Increase;
		int m_Space;
		int m_NodeCount;
		bool m_File;
		bool m_FirstNode;

	public:

		/**
		 * Construktor
		 */
		BaseXMLWriter( const char* filename, int space = 3);

		/**
		 * Construktor
		 */
		BaseXMLWriter( std::ostream& out, int space = 3 );

		/**
		 * Destruktor
		 */
		virtual ~BaseXMLWriter();

		/*
		 * begin an new node
		 */
		void BeginNode( const std::string& name );

		/**
		 * close an open node
		 */
		void EndNode( );

		/**
		 * Write string Property
		 */
		void WriteProperty( const std::string& key, const char* value ) const;

		/**
		 * Write string Property
		 */
		void WriteProperty( const std::string& key, const std::string& value ) const;

		/**
		 * Write int Property
		 */
		void WriteProperty( const std::string& key, int value ) const;

		/**
		 * Write float Property
		 */
		void WriteProperty( const std::string& key, float value ) const;

		/**
		 * Write double Property
		 */
		void WriteProperty( const std::string& key, double value ) const;

		/**
		 * Write bool Property
		 */
		void WriteProperty( const std::string& key, bool value ) const;

		/**
		 * Write comment
		 */
		void WriteComment( const std::string& key );
	
		/**
		 * retun the current deph
		 */
		int GetCurrentDeph() const;

		/**
		 * Get the current count of nodes
		 */
		int GetNodeCount() const;

		/**
		 * Get the space
		 */
		int GetSpace() const;

		/**
		 *
		 */
		void SetSpace( int space );

protected:
		/**
		 * replace char < and > through { and }
		 */
		const char* ConvertString( const char* string ) const;
	};
}
#endif // MITK_BASE_XML_WRITER
