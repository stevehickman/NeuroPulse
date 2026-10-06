//! The field table (common/npps/fields.json), loaded once. Every modality's fields, their
//! spellings, kinds and defaults are data here and nowhere in code.

use serde_json::Value;
use std::sync::OnceLock;

static RAW: &str = include_str!("../../npps/fields.json");

pub struct Table(pub Value);

pub fn table() -> &'static Table {
    static T: OnceLock<Table> = OnceLock::new();
    T.get_or_init(|| Table(serde_json::from_str(RAW).expect("common/npps/fields.json is valid JSON")))
}

impl Table {
    pub fn alias(&self, key: &str) -> Option<&str> {
        self.0["aliases"].get(key).and_then(Value::as_str)
    }

    fn list_has(&self, section: &str, key: &str, modality: &str) -> bool {
        self.0[section]
            .get(key)
            .and_then(Value::as_array)
            .map_or(false, |a| a.iter().any(|m| m.as_str() == Some(modality)))
    }

    /// The canonical key `intensity` resolves to for this modality, if any.
    pub fn intensity_target(&self, modality: &str) -> Option<&str> {
        let obj = self.0["intensity_alias"].as_object()?;
        obj.iter()
            .filter(|(k, _)| !k.starts_with('$'))
            .find(|(_, v)| v.as_array().map_or(false, |a| a.iter().any(|m| m.as_str() == Some(modality))))
            .map(|(k, _)| k.as_str())
    }

    pub fn percent_refused(&self, key: &str, modality: &str) -> bool {
        self.list_has("percent_refused", key, modality)
    }

    pub fn retired_wavelength(&self, value: &str) -> Option<Vec<&str>> {
        self.0["retired_wavelengths"]
            .get(value)
            .and_then(Value::as_array)
            .map(|a| a.iter().filter_map(Value::as_str).collect())
    }

    pub fn is_block_key(&self, key: &str) -> bool {
        self.0["block_keys"].as_array().map_or(false, |a| a.iter().any(|k| k.as_str() == Some(key)))
    }

    pub fn modality(&self, name: &str) -> Option<&Value> {
        self.0["modalities"].get(name)
    }

    /// The `quantity` field a short or canonical spelling names, for this modality.
    pub fn quantity_for(&self, modality: &str, key: &str) -> Option<&Value> {
        let m = self.modality(modality)?;
        m["fields"].as_array()?.iter().find(|f| {
            f["kind"] == "quantity" && (f["key"] == key || f["short"] == key)
        })
    }
}
