package com.smartvest.command.data

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.smartvest.command.databinding.ItemAlertBinding
import com.smartvest.command.util.ThemeManager

class AlertAdapter(private val items: List<AlertItem>) :
    RecyclerView.Adapter<AlertAdapter.AlertViewHolder>() {

    inner class AlertViewHolder(val binding: ItemAlertBinding) :
        RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AlertViewHolder {
        val binding = ItemAlertBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return AlertViewHolder(binding)
    }

    override fun onBindViewHolder(holder: AlertViewHolder, position: Int) {
        val item = items[position]
        holder.binding.txtAlertTitle.text = item.title
        holder.binding.txtAlertTime.text = item.detail
        holder.binding.imgAlertIcon.setImageResource(item.iconRes)
        // Title stays a fixed red (matches the website's hazard styling in both
        // themes); only the detail line follows the current light/dark theme.
        ThemeManager.applyTextMain(holder.binding.txtAlertTime)
    }

    override fun getItemCount(): Int = items.size
}
