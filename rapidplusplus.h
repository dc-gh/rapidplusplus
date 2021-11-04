#pragma once

#include "rapidxml_ext.hpp"

#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <unordered_map>
#include <map>
#include <assert.h>
#include <algorithm>
#include <memory>

namespace XML
{
  class Attribute
  {
  public:

    Attribute() = default;

    Attribute(std::string name, std::string value) :
      m_name(std::move(name)),
      m_value(std::move(value))
    {
    }

    std::string Name() const
    {
      return m_name;
    }

    void SetName(std::string name)
    {
      m_name = std::move(name);
      if (m_xml_attribute != nullptr)
        m_xml_attribute->name(m_name.c_str());
    }

    std::string Value() const
    {
      return m_value;
    }

    void SetValue(std::string value)
    {
      m_value = std::move(value);
      if (m_xml_attribute != nullptr)
        m_xml_attribute->value(m_value.c_str());
    }

    bool IsValid() const
    {
      return m_xml_attribute != nullptr;
    }

  private:

    friend class Element;

    std::string m_name;
    std::string m_value;
    rapidxml::xml_attribute<>* m_xml_attribute = nullptr;

    Attribute(rapidxml::xml_attribute<>* attribute)
    {
      assert(attribute != nullptr);

      m_xml_attribute = attribute;

      m_name = std::string(attribute->name(), attribute->name_size());
      m_value = std::string(attribute->value(), attribute->value_size());
    }
  };

  class Element
  {
  public:

    Element() = default;

    Element(std::string name, std::string value = std::string()) :
      m_name(std::move(name)),
      m_value(std::move(value))
    {
    }

    Element(const Element&) = default;

    Element(Element&&) noexcept = default;

    Element& operator=(const Element& other) = default;

    Element& operator=(Element&& other) noexcept = default;

    ~Element() = default;

    std::string Value() const
    {
      return m_value;
    }

    std::string Name() const
    {
      return m_name;
    }

    void SetName(std::string name)
    {
      m_name = std::move(name);
      if (m_xml_node != nullptr)
        m_xml_node->name(m_name.c_str());
    }

    const std::vector<Attribute>& Attributes() const
    {
      return m_attributes;
    }

    void AppendAttribute(const Attribute& attribute)
    {
      assert(m_xml_node != nullptr);
      auto document = m_document.lock();
      assert(document != nullptr);

      char* name = document->allocate_string(attribute.m_name.c_str(), attribute.m_name.length() + 1);
      char* value = document->allocate_string(attribute.m_value.c_str(), attribute.m_value.length() + 1);
      const auto& rapid_attribute = document->allocate_attribute(name, value);

      m_attributes.emplace_back(Attribute(rapid_attribute));
      m_xml_node->append_attribute(rapid_attribute);
    }

#if 0
    template <typename... Args>
    void AppendAttribute(Args&&... args)
    {
      const Attribute attribute(std::forward<Args>(args)...);
      AppendAttribute(attribute);
    }
#endif

    void AppendElement(const Element& element)
    {
      if (IsValid() == false)
      {
        assert(IsValid());
        return;
      }
      
      auto document = m_document.lock();
      assert(document != nullptr);

      char* name = document->allocate_string(element.m_name.c_str(), element.m_name.length() + 1);
      char* value = nullptr;
      if (element.Value().length())
      {
        value = document->allocate_string(element.m_value.c_str(), element.m_value.length() + 1);
      }

      const auto& rapid_element = document->allocate_node(rapidxml::node_element, name, value, element.m_name.length() + 1, element.m_value.length() + 1);
      m_xml_node->append_node(rapid_element);
      element.m_xml_node = rapid_element;
      element.m_document = m_document;
    }

    void AppendCDATA(const std::string& cdata)
    {
      if (IsValid() == false)
      {
        assert(IsValid());
        return;
      }

      auto document = m_document.lock();
      assert(document != nullptr);

      char* cdata_str = document->allocate_string(cdata.c_str(), cdata.length() + 1);

      const auto& rapid_cdata = document->allocate_node(rapidxml::node_cdata, cdata_str);
      m_xml_node->append_node(rapid_cdata);
    }

#if 0
    template <typename... Args>
    void AppendElement(Args&&... args)
    {
      const Element element(std::forward<Args>(args)...);
      AppendElement(element);
    }
#endif

    bool IsValid() const
    {
      return m_xml_node != nullptr;
    }

    Element Child(const std::string& node = std::string()) const
    {
      if (m_xml_node == nullptr)
        return {};

      return Element(m_document, m_xml_node->first_node(node.c_str()));
    }

    /**
    * Returns all children of the current Element. Children can be optionally filtered on the
    * Element name.
    */
    std::vector<Element> Children(const std::string& node = std::string()) const
    {
      if (m_xml_node == nullptr)
        return {};

      auto child_node = m_xml_node->first_node(node.empty() ? nullptr: node.c_str());

      std::vector<Element> result;
      while (child_node != nullptr)
      {
        result.emplace_back(Element(m_document.lock(), child_node));
        child_node = child_node->next_sibling(node.empty() ? nullptr : node.c_str());
      }

      return result;
    }

  private:

    friend class Document;

    std::vector<Attribute> m_attributes;
    std::string m_value;
    std::string m_name;
    bool m_valid = false;
    mutable rapidxml::xml_node<>* m_xml_node = nullptr;
    mutable std::weak_ptr<rapidxml::xml_document<>> m_document;

    Element(const std::weak_ptr<rapidxml::xml_document<>>& document, rapidxml::xml_node<>* node)
    {
      if (node == nullptr)
        return;

      m_xml_node = node;
      m_document = document;

      m_valid = true;
      m_name = std::string(node->name(), node->name_size());
      m_value = std::string(node->value(), node->value_size());

      auto attribute = node->first_attribute();
      while (attribute != nullptr)
      {
        m_attributes.emplace_back(Attribute(attribute));
        attribute = attribute->next_attribute();
      }
    }
  };

  class Document
  {
  public:

    Document() = default;

    Document(const std::string& xml_str)
    {
      m_xml_text = std::vector<char>(xml_str.begin(), xml_str.end());
      m_xml_text.push_back('\0');
      m_document->parse<rapidxml::parse_full>(&m_xml_text[0]);

      auto node = m_document->first_node();
      while (node != nullptr)
      {
        switch (node->type())
        {
        case rapidxml::node_type::node_declaration:
          m_declaration = Element(m_document, node);
          break;

        case rapidxml::node_type::node_element:
          m_root = Element(m_document, node);
          break;

        default:
          break;
        }

        node = node->next_sibling();
      }
    }

    Document(const Document& other) :
      Document(other.ToString())
    {
    }

    Document(Document&& other) noexcept :
      m_xml_text(std::move(other.m_xml_text)),
      m_document(std::move(other.m_document)),
      m_declaration(std::move(other.m_declaration)),
      m_root(std::move(other.m_root))
    {
    }

    Document& operator=(const Document& other)
    {
      m_document = std::make_shared<rapidxml::xml_document<>>();

      m_xml_text = other.m_xml_text;
      m_document->parse<rapidxml::parse_full>(&m_xml_text[0]);

      return *this;
    }

    Document& operator=(Document&& other) noexcept
    {
      m_document = std::move(other.m_document);
      m_xml_text = std::move(other.m_xml_text);
      m_declaration = std::move(other.m_declaration);
      m_root = std::move(other.m_root);

      return *this;
    }

    ~Document() = default;

    Element RootElement() const
    {
      return m_root;
    }

    void AddRootElement(const XML::Element& root_element)
    {
      char* name = m_document->allocate_string(root_element.Name().c_str(), root_element.Name().length() + 1);
      char* value = nullptr;
      if (root_element.Value().length())
      {
        value = m_document->allocate_string(root_element.m_value.c_str(), root_element.m_value.length() + 1);
      }
      rapidxml::xml_node<>* element = m_document->allocate_node(rapidxml::node_element, name, value);
      m_document->append_node(element);

      root_element.m_document = m_document;
      root_element.m_xml_node = element;

      m_root = root_element;
    }

    Element Declaration() const
    {
      return m_declaration;
    }

    std::string ToString() const
    {
      std::string s;
      // rapidxml::print(std::back_inserter(s), *m_document.get(), 0);
      return s;
    }

  private:

    std::vector<char> m_xml_text;
    std::shared_ptr<rapidxml::xml_document<>> m_document = std::make_shared<rapidxml::xml_document<>>();
    Element m_root;
    Element m_declaration;

    friend std::ostream& operator<<(std::ostream& os, const Document& dt);
  };

  std::ostream& operator<<(std::ostream& os, const Document& dt)
  {
    if (dt.m_document != nullptr)
    {
      os << *dt.m_document.get();
    }
    return os;
  }
}
